#include "../include/precompiled.h"
#include "../include/Config.h"
#include "../include/GitHubOAuth.h"
#include "../include/JWT.h"
#include "../include/SimpleDB.h"
#include "../include/XTunnelSimple.h"
#include "../include/AutoSessionManager.h"

#include <iostream>
#include <sstream>
#include <map>
#include <ctime>
#include <thread>
#include <vector>
#include <algorithm>

#ifdef _WIN32
#include "win_clean.h"
#pragma comment(lib, "ws2_32.lib")
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#endif

using namespace std;
using json = nlohmann::json;

// ========== ПРОСТОЙ HTTP СЕРВЕР НА СИ ==========

class SimpleHTTPServer {
private:
    int port;
    bool running;
    SOCKET server_socket;
    thread server_thread;
    
    struct Route {
        string method;
        function<string(const string&, const map<string, string>&)> handler;
    };
    
    map<string, Route> routes;
    
    map<string, string> parse_headers(const string& header_text) {
        map<string, string> headers;
        istringstream stream(header_text);
        string line;
        
        while (getline(stream, line) && line != "\r" && !line.empty()) {
            size_t colon = line.find(':');
            if (colon != string::npos) {
                string key = line.substr(0, colon);
                string value = line.substr(colon + 2);
                if (!value.empty() && value.back() == '\r') value.pop_back();
                headers[key] = value;
            }
        }
        return headers;
    }
    
    void handle_client(SOCKET client_socket) {
        char buffer[8192];
        int bytes_received = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
        
        if (bytes_received > 0) {
            buffer[bytes_received] = '\0';
            string request(buffer);
            
            // Parse request line
            istringstream req_stream(request);
            string request_line;
            getline(req_stream, request_line);
            
            string method, path, version;
            istringstream line_stream(request_line);
            line_stream >> method >> path >> version;
            
            // Store full path for query parsing
            string full_path = path;
            
            // Remove query string from path for routing
            size_t qmark = path.find('?');
            if (qmark != string::npos) path = path.substr(0, qmark);
            
            // Parse headers
            string headers_text;
            string line;
            while (getline(req_stream, line) && line != "\r" && !line.empty()) {
                headers_text += line + "\n";
            }
            
            auto headers = parse_headers(headers_text);
            headers["Request-Path"] = full_path; // Store full path with query
            
            // Get body
            string body;
            if (headers.count("Content-Length")) {
                int content_length = stoi(headers["Content-Length"]);
                body.resize(content_length);
                req_stream.read(&body[0], content_length);
            }
            
            string response;
            
            // Handle OPTIONS for CORS
            if (method == "OPTIONS") {
                response = "HTTP/1.1 200 OK\r\n";
                response += "Access-Control-Allow-Origin: *\r\n";
                response += "Access-Control-Allow-Methods: GET, POST, PUT, DELETE, OPTIONS\r\n";
                response += "Access-Control-Allow-Headers: Content-Type, Authorization\r\n";
                response += "Content-Length: 0\r\n\r\n";
            }
            // Find route
            else if (routes.count(path) && routes[path].method == method) {
                string response_body = routes[path].handler(body, headers);
                
                response = "HTTP/1.1 200 OK\r\n";
                response += "Content-Type: application/json\r\n";
                response += "Access-Control-Allow-Origin: *\r\n";
                response += "Content-Length: " + to_string(response_body.length()) + "\r\n\r\n";
                response += response_body;
            } else {
                // 404
                json error = {{"error", "Not found"}, {"path", path}};
                string error_body = error.dump();
                response = "HTTP/1.1 404 Not Found\r\n";
                response += "Content-Type: application/json\r\n";
                response += "Access-Control-Allow-Origin: *\r\n";
                response += "Content-Length: " + to_string(error_body.length()) + "\r\n\r\n";
                response += error_body;
            }
            
            send(client_socket, response.c_str(), response.length(), 0);
        }
        
#ifdef _WIN32
        closesocket(client_socket);
#else
        close(client_socket);
#endif
    }
    
public:
    SimpleHTTPServer(int port = 8081) : port(port), running(false), server_socket(INVALID_SOCKET) {
#ifdef _WIN32
        WSADATA wsa_data;
        WSAStartup(MAKEWORD(2, 2), &wsa_data);
#endif
    }
    
    ~SimpleHTTPServer() {
        stop();
#ifdef _WIN32
        WSACleanup();
#endif
    }
    
    void route(const string& method, const string& path, 
               function<string(const string&, const map<string, string>&)> handler) {
        routes[path] = {method, handler};
    }
    
    void start() {
        server_socket = socket(AF_INET, SOCK_STREAM, 0);
        
        // Allow reuse
        int opt = 1;
#ifdef _WIN32
        setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt));
#else
        setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
#endif
        
        sockaddr_in server_addr;
        server_addr.sin_family = AF_INET;
        server_addr.sin_addr.s_addr = INADDR_ANY;
        server_addr.sin_port = htons(port);
        
        if (bind(server_socket, (sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
            cerr << "Bind failed" << endl;
            return;
        }
        
        listen(server_socket, 10);
        
        running = true;
        cout << "✅ HTTP Server started on port " << port << endl;
        
        server_thread = thread([this]() {
            while (running) {
                sockaddr_in client_addr;
                socklen_t client_len = sizeof(client_addr);
                
                SOCKET client_socket = accept(server_socket, (sockaddr*)&client_addr, &client_len);
                if (client_socket != INVALID_SOCKET) {
                    thread(&SimpleHTTPServer::handle_client, this, client_socket).detach();
                }
            }
        });
    }
    
    void stop() {
        running = false;
        if (server_socket != INVALID_SOCKET) {
#ifdef _WIN32
            closesocket(server_socket);
#else
            close(server_socket);
#endif
        }
        if (server_thread.joinable()) server_thread.join();
    }
};

// ========== API ФУНКЦИИ ==========

string extract_token(const map<string, string>& headers) {
    auto it = headers.find("Authorization");
    if (it == headers.end()) return "";
    
    const string& auth = it->second;
    if (auth.find("Bearer ") == 0) {
        return auth.substr(7);
    }
    return "";
}

// Функция извлечения query параметров
string get_query_param(const map<string, string>& headers, const string& param) {
    auto it = headers.find("Request-Path");
    if (it == headers.end()) return "";
    
    const string& full_path = it->second;
    size_t start = full_path.find(param + "=");
    if (start == string::npos) return "";
    
    start += param.length() + 1;
    size_t end = full_path.find("&", start);
    if (end == string::npos) end = full_path.length();
    
    return full_path.substr(start, end - start);
}

// Простая функция хеширования пароля (в продакшене используйте bcrypt/scrypt)
string hash_password(const string& password) {
    // Простой хеш для демонстрации. ЗАМЕНИТЕ на настоящий хеш в продакшене!
    hash<string> hasher;
    return to_string(hasher(password + "salt_123"));
}

// ========== НОВЫЙ API СЕРВЕР ==========

void runFullAPIServer(int port, JWT& jwt, SimpleDB& db, GitHubOAuth& github) {
    SimpleHTTPServer server(port);
    
    // Создаем менеджер сессий для login_token
    AuthSessionManager sessionManager;
    
    // ========== ВСЕ ЭНДПОИНТЫ ==========
    
    // 1. Health check
    server.route("GET", "/health", [](const string& body, const map<string, string>& headers) {
        return json{{"status", "ok"}, {"service", "auth"}, {"timestamp", time(nullptr)}}.dump();
    });
    
    // 2. Discovery endpoint
    server.route("GET", "/api/discovery", [](const string& body, const map<string, string>& headers) {
        return json{
            {"service", "auth_module"},
            {"version", "2.1-full-registration"},
            {"timestamp", time(nullptr)},
            {"endpoints", {
                {{"method", "GET"}, {"path", "/health"}, {"description", "Health check"}},
                {{"method", "POST"}, {"path", "/api/auth/init"}, {"description", "Initialize auth session (GitHub)"}},
                {{"method", "GET"}, {"path", "/api/auth/status"}, {"description", "Check auth status"}},
                {{"method", "GET"}, {"path", "/api/auth/callback"}, {"description", "GitHub OAuth callback"}},
                {{"method", "POST"}, {"path", "/api/auth/validate"}, {"description", "Validate JWT token"}},
                {{"method", "POST"}, {"path", "/api/auth/refresh"}, {"description", "Refresh JWT token"}},
                {{"method", "POST"}, {"path", "/api/auth/logout"}, {"description", "Logout (revoke refresh token)"}},
                {{"method", "POST"}, {"path", "/api/users/register"}, {"description", "Register new user (password)"}},
                {{"method", "POST"}, {"path", "/api/users/login"}, {"description", "Login with password"}},
                {{"method", "GET"}, {"path", "/api/users/list"}, {"description", "List all users (admin only)"}}
            }}
        }.dump();
    });
    
    // 3. Инициализация авторизации через GitHub
    server.route("POST", "/api/auth/init", [&](const string& body, const map<string, string>& headers) {
        try {
            // Создаем новую сессию с login_token
            string login_token = sessionManager.createSession();
            
            // Генерируем URL для GitHub OAuth с login_token в state
            string auth_url = github.getAuthorizationUrlWithToken(login_token);
            
            json response = {
                {"success", true},
                {"token", login_token},
                {"auth_url", auth_url},
                {"expires_in", 300}, // 5 минут
                {"timestamp", time(nullptr)}
            };
            
            cout << "[Auth] Created GitHub login_token: " << login_token << endl;
            return response.dump();
            
        } catch (const exception& e) {
            return json{{"success", false}, {"error", e.what()}}.dump();
        }
    });
    
    // 4. Callback от GitHub OAuth
    server.route("GET", "/api/auth/callback", [&](const string& body, const map<string, string>& headers) {
        // Парсим query параметры из Request-Path
        string code = get_query_param(headers, "code");
        string state = get_query_param(headers, "state");
        
        cout << "[Auth] GitHub callback received. Code: " << (code.empty() ? "empty" : "present")
             << ", State: " << state << endl;
        
        if (code.empty() || state.empty()) {
            return json{{"error", "Missing code or state parameters"}}.dump();
        }
        
        // Проверяем что state начинается с token_
        if (state.find("token_") != 0) {
            return json{{"error", "Invalid state format. Expected 'token_<login_token>'"}}.dump();
        }
        
        string login_token = state.substr(6); // Убираем "token_"
        
        cout << "[Auth] Processing GitHub callback for token: " << login_token << endl;
        
        // Сохраняем код в сессию
        if (!sessionManager.updateSessionWithCode(login_token, code)) {
            return json{{"error", "Invalid or expired login_token"}}.dump();
        }
        
        // Асинхронно обрабатываем OAuth (не блокируем ответ)
        thread([&, login_token, code]() {
            try {
                cout << "[Auth] Processing OAuth async for token: " << login_token << endl;
                
                // Получаем access token от GitHub
                string github_access_token = github.getAccessToken(code);
                if (github_access_token.empty()) {
                    cerr << "[Auth] Failed to get GitHub access token for token: " << login_token << endl;
                    sessionManager.setSessionDenied(login_token);
                    return;
                }
                
                cout << "[Auth] Got GitHub access token for token: " << login_token << endl;
                
                // Получаем информацию о пользователе
                GitHubUser github_user = github.getUserInfo(github_access_token);
                if (github_user.id.empty()) {
                    cerr << "[Auth] Failed to get GitHub user info for token: " << login_token << endl;
                    sessionManager.setSessionDenied(login_token);
                    return;
                }
                
                cout << "[Auth] GitHub user: " << github_user.login 
                     << " (" << github_user.name << ")" << endl;
                
                // Создаем или обновляем пользователя в базе
                User user = db.createOrUpdateUser(
                    github_user.id,
                    github_user.login,
                    github_user.email,
                    github_user.name,
                    "1", // default course
                    ""   // no password for GitHub auth
                );
                
                if (user.id.empty()) {
                    cerr << "[Auth] Failed to save user to database for token: " << login_token << endl;
                    sessionManager.setSessionDenied(login_token);
                    return;
                }
                
                cout << "[Auth] User saved to DB: " << user.username << " (ID: " << user.id << ")" << endl;
                
                // Генерируем JWT токены
                map<string, string> payload = {
                    {"user_id", user.id},
                    {"username", user.username},
                    {"email", user.email},
                    {"fullname", user.full_name},
                    {"role", user.role},
                    {"course", user.course}
                };
                
                string access_token = jwt.generateToken(payload);
                string refresh_token = jwt.generateRefreshToken();
                
                // Сохраняем refresh token в базу
                db.saveRefreshToken(user.id, refresh_token);
                
                // Отмечаем сессию как успешную
                bool success = sessionManager.setSessionSuccess(
                    login_token,
                    access_token,
                    refresh_token,
                    user.id,
                    github_user.id
                );
                
                if (success) {
                    cout << "[Auth] OAuth completed successfully for user: " << user.username 
                         << " (token: " << login_token << ")" << endl;
                } else {
                    cerr << "[Auth] Failed to mark session as success for token: " << login_token << endl;
                }
                     
            } catch (const exception& e) {
                cerr << "[Auth] Error processing OAuth for token " << login_token << ": " << e.what() << endl;
                sessionManager.setSessionDenied(login_token);
            }
        }).detach();
        
        return json{{"status", "processing"}, {"message", "Authentication in progress. Poll /api/auth/status"}}.dump();
    });
    
    // 5. Проверка статуса авторизации
    server.route("GET", "/api/auth/status", [&](const string& body, const map<string, string>& headers) {
        // Извлекаем token из query параметров
        string token = get_query_param(headers, "token");
        
        if (token.empty()) {
            return json{{"error", "Missing token parameter. Use /api/auth/status?token=<login_token>"}}.dump();
        }
        
        cout << "[Auth] Checking status for token: " << token << endl;
        
        AuthSession session = sessionManager.getSession(token);
        
        json response = {
            {"token", token},
            {"status", session.status},
            {"timestamp", time(nullptr)}
        };
        
        if (session.status == "success") {
            response["access_token"] = session.access_token;
            response["refresh_token"] = session.refresh_token;
            response["user_id"] = session.user_id;
            response["github_id"] = session.github_id;
            
            // Удаляем сессию после успешного получения
            sessionManager.removeSession(token);
            cout << "[Auth] Token " << token << " marked as success and removed" << endl;
        }
        else if (session.status == "expired") {
            sessionManager.removeSession(token);
            cout << "[Auth] Token " << token << " expired and removed" << endl;
        }
        else if (session.status == "denied") {
            sessionManager.removeSession(token);
            cout << "[Auth] Token " << token << " denied and removed" << endl;
        }
        else if (session.status == "pending") {
            response["message"] = "Authorization still in progress";
            cout << "[Auth] Token " << token << " still pending" << endl;
        }
        
        return response.dump();
    });
    
    // 6. Валидация JWT токена
    server.route("POST", "/api/auth/validate", [&](const string& body, const map<string, string>& headers) {
        string token = extract_token(headers);
        if (token.empty()) {
            // Также пробуем из тела запроса
            try {
                if (!body.empty()) {
                    auto data = json::parse(body);
                    if (data.contains("access_token")) {
                        token = data["access_token"];
                    }
                }
            } catch (...) {}
            
            if (token.empty()) {
                return json{{"valid", false}, {"error", "No token provided"}}.dump();
            }
        }
        
        cout << "[Auth] Validating JWT token" << endl;
        
        auto claims = jwt.validateToken(token);
        if (claims.empty()) {
            return json{{"valid", false}, {"error", "Invalid or expired token"}}.dump();
        }
        
        json user_data;
        for (const auto& claim : claims) {
            user_data[claim.first] = claim.second;
        }
        
        return json{{"valid", true}, {"user", user_data}}.dump();
    });
    
    // 7. Обновление токенов
    server.route("POST", "/api/auth/refresh", [&](const string& body, const map<string, string>& headers) {
        try {
            auto data = json::parse(body);
            string refresh_token = data["refresh_token"];
            string user_id = data["user_id"];
            
            if (refresh_token.empty() || user_id.empty()) {
                return json{{"error", "Missing refresh_token or user_id"}}.dump();
            }
            
            cout << "[Auth] Refreshing tokens for user: " << user_id << endl;
            
            if (!db.validateRefreshToken(user_id, refresh_token)) {
                return json{{"error", "Invalid refresh token"}}.dump();
            }
            
            User user = db.getUserById(user_id);
            if (user.id.empty()) {
                return json{{"error", "User not found"}}.dump();
            }
            
            // Генерируем новые токены
            map<string, string> payload = {
                {"user_id", user.id},
                {"username", user.username},
                {"email", user.email},
                {"fullname", user.full_name},
                {"role", user.role},
                {"course", user.course}
            };
            
            string new_access_token = jwt.generateToken(payload);
            string new_refresh_token = jwt.generateRefreshToken();
            
            // Отзываем старый и сохраняем новый refresh token
            db.revokeRefreshToken(user_id);
            db.saveRefreshToken(user_id, new_refresh_token);
            
            cout << "[Auth] Tokens refreshed for user: " << user.username << endl;
            
            return json{
                {"success", true},
                {"access_token", new_access_token},
                {"refresh_token", new_refresh_token}
            }.dump();
        } catch (const exception& e) {
            return json{{"error", "Invalid request: " + string(e.what())}}.dump();
        }
    });
    
    // 8. Логаут
    server.route("POST", "/api/auth/logout", [&](const string& body, const map<string, string>& headers) {
        try {
            auto data = json::parse(body);
            string refresh_token = data["refresh_token"];
            string user_id = data["user_id"];
            
            if (refresh_token.empty() || user_id.empty()) {
                return json{{"success", false}, {"error", "Missing refresh_token or user_id"}}.dump();
            }
            
            cout << "[Auth] Logout requested for user: " << user_id << endl;
            
            // Проверяем что токен существует перед отзывом
            if (!db.validateRefreshToken(user_id, refresh_token)) {
                return json{{"success", false}, {"error", "Invalid token"}}.dump();
            }
            
            // Отзываем токен
            db.revokeRefreshToken(user_id);
            
            cout << "[Auth] User " << user_id << " logged out (refresh token revoked)" << endl;
            return json{{"success", true}, {"message", "Logged out successfully"}}.dump();
            
        } catch (const exception& e) {
            return json{{"success", false}, {"error", "Invalid request"}}.dump();
        }
    });
    
    // ========== НОВЫЕ ЭНДПОИНТЫ: РЕГИСТРАЦИЯ И ЛОГИН ==========
    
    // 9. Регистрация с паролем (без GitHub)
    server.route("POST", "/api/users/register", [&](const string& body, const map<string, string>& headers) {
        try {
            auto data = json::parse(body);
            
            // Проверка обязательных полей
            if (!data.contains("username") || !data.contains("email") || 
                !data.contains("password") || !data.contains("full_name")) {
                return json{{"success", false}, {"error", "Missing required fields"}}.dump();
            }
            
            string username = data["username"];
            string email = data["email"];
            string password = data["password"];
            string full_name = data["full_name"];
            string course = data.value("course", "1");
            string role = data.value("role", "student");
            
            // Валидация
            if (username.empty() || email.empty() || password.empty() || full_name.empty()) {
                return json{{"success", false}, {"error", "Fields cannot be empty"}}.dump();
            }
            
            if (password.length() < 6) {
                return json{{"success", false}, {"error", "Password must be at least 6 characters"}}.dump();
            }
            
            // Проверяем, нет ли уже такого пользователя
            auto all_users = db.getAllUsers();
            for (const auto& user : all_users) {
                if (user.username == username) {
                    return json{{"success", false}, {"error", "Username already exists"}}.dump();
                }
                if (user.email == email) {
                    return json{{"success", false}, {"error", "Email already registered"}}.dump();
                }
            }
            
            // Хешируем пароль
            string password_hash = hash_password(password);
            
            cout << "[Auth] Registering new user: " << username << " (" << email << ")" << endl;
            
            // Создаем пользователя
            User user = db.createUserWithPassword(
                username,
                email,
                full_name,
                password_hash,
                course,
                role
            );
            
            if (user.id.empty()) {
                return json{{"success", false}, {"error", "Failed to create user"}}.dump();
            }
            
            // Создаем сессию для немедленного логина
            string login_token = sessionManager.createSession();
            
            // Генерируем JWT токены
            map<string, string> payload = {
                {"user_id", user.id},
                {"username", user.username},
                {"email", user.email},
                {"fullname", user.full_name},
                {"role", user.role},
                {"course", user.course}
            };
            
            string access_token = jwt.generateToken(payload);
            string refresh_token = jwt.generateRefreshToken();
            
            // Сохраняем refresh token
            db.saveRefreshToken(user.id, refresh_token);
            
            // Помечаем сессию как успешную
            sessionManager.setSessionSuccess(
                login_token,
                access_token,
                refresh_token,
                user.id,
                "" // Нет GitHub ID
            );
            
            cout << "[Auth] User registered successfully: " << username << " (ID: " << user.id << ")" << endl;
            
            return json{
                {"success", true},
                {"message", "User registered successfully"},
                {"user_id", user.id},
                {"username", user.username},
                {"access_token", access_token},
                {"refresh_token", refresh_token},
                {"login_token", login_token} // Для совместимости с polling механизмом
            }.dump();
            
        } catch (const exception& e) {
            return json{{"success", false}, {"error", "Invalid request: " + string(e.what())}}.dump();
        }
    });
    
    // 10. Логин с паролем
    server.route("POST", "/api/users/login", [&](const string& body, const map<string, string>& headers) {
        try {
            auto data = json::parse(body);
            
            if (!data.contains("username") || !data.contains("password")) {
                return json{{"success", false}, {"error", "Missing username or password"}}.dump();
            }
            
            string username = data["username"];
            string password = data["password"];
            
            // Ищем пользователя
            auto all_users = db.getAllUsers();
            User found_user;
            
            for (const auto& user : all_users) {
                if (user.username == username) {
                    found_user = user;
                    break;
                }
            }
            
            if (found_user.id.empty()) {
                return json{{"success", false}, {"error", "User not found"}}.dump();
            }
            
            // Проверяем пароль (упрощенно - в реальности нужно сравнивать хеши)
            if (found_user.password_hash != hash_password(password)) {
                return json{{"success", false}, {"error", "Invalid password"}}.dump();
            }
            
            cout << "[Auth] User login successful: " << username << endl;
            
            // Создаем сессию
            string login_token = sessionManager.createSession();
            
            // Генерируем JWT токены
            map<string, string> payload = {
                {"user_id", found_user.id},
                {"username", found_user.username},
                {"email", found_user.email},
                {"fullname", found_user.full_name},
                {"role", found_user.role},
                {"course", found_user.course}
            };
            
            string access_token = jwt.generateToken(payload);
            string refresh_token = jwt.generateRefreshToken();
            
            // Сохраняем refresh token
            db.saveRefreshToken(found_user.id, refresh_token);
            
            // Помечаем сессию как успешную
            sessionManager.setSessionSuccess(
                login_token,
                access_token,
                refresh_token,
                found_user.id,
                found_user.github_id
            );
            
            return json{
                {"success", true},
                {"message", "Login successful"},
                {"access_token", access_token},
                {"refresh_token", refresh_token},
                {"login_token", login_token},
                {"user_id", found_user.id},
                {"username", found_user.username}
            }.dump();
            
        } catch (const exception& e) {
            return json{{"success", false}, {"error", "Invalid request: " + string(e.what())}}.dump();
        }
    });
    
    // 11. Список всех пользователей (только для админов)
    server.route("GET", "/api/users/list", [&](const string& body, const map<string, string>& headers) {
        // Проверка авторизации
        string token = extract_token(headers);
        auto claims = jwt.validateToken(token);
        
        if (claims.empty() || claims["role"] != "admin") {
            return json{{"success", false}, {"error", "Admin access required"}}.dump();
        }
        
        auto users = db.getAllUsers();
        json users_json = json::array();
        
        for (const auto& user : users) {
            users_json.push_back({
                {"id", user.id},
                {"username", user.username},
                {"email", user.email},
                {"full_name", user.full_name},
                {"role", user.role},
                {"course", user.course},
                {"has_github", !user.github_id.empty()}
            });
        }
        
        return json{{"success", true}, {"users", users_json}, {"count", users.size()}}.dump();
    });
    
    // 12. Получить информацию о текущем пользователе
    server.route("GET", "/api/users/me", [&](const string& body, const map<string, string>& headers) {
        string token = extract_token(headers);
        if (token.empty()) {
            return json{{"success", false}, {"error", "No token provided"}}.dump();
        }
        
        auto claims = jwt.validateToken(token);
        if (claims.empty()) {
            return json{{"success", false}, {"error", "Invalid token"}}.dump();
        }
        
        User user = db.getUserById(claims["user_id"]);
        if (user.id.empty()) {
            return json{{"success", false}, {"error", "User not found"}}.dump();
        }
        
        return json{
            {"success", true},
            {"user", {
                {"id", user.id},
                {"username", user.username},
                {"email", user.email},
                {"full_name", user.full_name},
                {"role", user.role},
                {"course", user.course},
                {"has_github", !user.github_id.empty()}
            }}
        }.dump();
    });
    
    // 13. Эндпоинт для отладки сессий
    server.route("GET", "/api/auth/debug/sessions", [&](const string& body, const map<string, string>& headers) {
        auto sessions = sessionManager.getAllSessions();
        json sessions_json = json::array();
        
        for (const auto& session : sessions) {
            time_t now = time(nullptr);
            int expires_in = session.expires_at - now;
            
            sessions_json.push_back({
                {"token", session.token},
                {"status", session.status},
                {"created_at", session.created_at},
                {"expires_at", session.expires_at},
                {"expires_in_seconds", expires_in},
                {"user_id", session.user_id},
                {"github_id", session.github_id}
            });
        }
        
        return json{{"sessions", sessions_json}, {"count", sessions.size()}}.dump();
    });
    
    // ========== ЗАПУСК СЕРВЕРА ==========
    
    cout << "\n========================================" << endl;
    cout << "🔐 AUTH MODULE v3.1 (FULL REGISTRATION)" << endl;
    cout << "🌐 Server Port: " << port << endl;
    cout << "========================================" << endl;
    cout << "📋 API Endpoints:" << endl;
    cout << endl;
    cout << "  AUTH VIA GITHUB:" << endl;
    cout << "    POST /api/auth/init" << endl;
    cout << "    GET  /api/auth/callback" << endl;
    cout << "    GET  /api/auth/status" << endl;
    cout << endl;
    cout << "  REGISTRATION & LOGIN:" << endl;
    cout << "    POST /api/users/register" << endl;
    cout << "    POST /api/users/login" << endl;
    cout << endl;
    cout << "  TOKEN MANAGEMENT:" << endl;
    cout << "    POST /api/auth/validate" << endl;
    cout << "    POST /api/auth/refresh" << endl;
    cout << "    POST /api/auth/logout" << endl;
    cout << endl;
    cout << "  USER MANAGEMENT:" << endl;
    cout << "    GET  /api/users/me" << endl;
    cout << "    GET  /api/users/list (admin only)" << endl;
    cout << endl;
    cout << "  UTILITY:" << endl;
    cout << "    GET  /health" << endl;
    cout << "    GET  /api/discovery" << endl;
    cout << "========================================\n" << endl;
    
    server.start();
    
    // Keep running
    while (true) {
        this_thread::sleep_for(chrono::seconds(10));
    }
}

// ========== ИНТЕРАКТИВНЫЙ РЕЖИМ ==========

void printHelp() {
    cout << "\n=== Auth Module Commands ===" << endl;
    cout << "1. help          - Show this help" << endl;
    cout << "2. test          - Test configuration" << endl;
    cout << "3. init          - Create login_token for GitHub auth" << endl;
    cout << "4. register      - Register new user (interactive)" << endl;
    cout << "5. login         - Login with username/password" << endl;
    cout << "6. status TOKEN  - Check auth status" << endl;
    cout << "7. validate TOKEN- Validate JWT token" << endl;
    cout << "8. users         - List all users" << endl;
    cout << "9. sessions      - Show active sessions (debug)" << endl;
    cout << "10. api-start    - Start API server" << endl;
    cout << "11. exit         - Exit program" << endl;
    cout << "============================\n" << endl;
}

void testConfiguration(Config& config, SimpleDB& db) {
    cout << "\n=== Configuration Test ===" << endl;
    cout << "GitHub Client ID: " << config.getGithubClientId() << endl;
    cout << "JWT Secret: " << (config.getJwtSecret().empty() ? "NOT SET" : "SET") << endl;
    cout << "Database file: " << config.getDbFile() << endl;
    
    auto users = db.getAllUsers();
    cout << "Database: " << users.size() << " users" << endl;
    
    if (!users.empty()) {
        cout << "First user: " << users[0].username << " (" << users[0].email << ")" << endl;
    }
    cout << "==========================\n" << endl;
}

void interactiveRegister(SimpleDB& db, JWT& jwt, AuthSessionManager& sessionManager) {
    cout << "\n=== User Registration ===" << endl;
    
    string username, email, password, full_name, course;
    
    cout << "Username: ";
    getline(cin, username);
    
    cout << "Email: ";
    getline(cin, email);
    
    cout << "Password: ";
    getline(cin, password);
    
    cout << "Full Name: ";
    getline(cin, full_name);
    
    cout << "Course (default: 1): ";
    getline(cin, course);
    if (course.empty()) course = "1";
    
    // Проверяем существование пользователя
    auto all_users = db.getAllUsers();
    for (const auto& user : all_users) {
        if (user.username == username) {
            cout << "❌ Username already exists!" << endl;
            return;
        }
        if (user.email == email) {
            cout << "❌ Email already registered!" << endl;
            return;
        }
    }
    
    // Хешируем пароль
    string password_hash = hash_password(password);
    
    // Создаем пользователя
    User user = db.createUserWithPassword(
        username,
        email,
        full_name,
        password_hash,
        course,
        "student"
    );
    
    if (user.id.empty()) {
        cout << "❌ Failed to create user!" << endl;
        return;
    }
    
    cout << "✅ User registered successfully!" << endl;
    cout << "User ID: " << user.id << endl;
    cout << "Username: " << user.username << endl;
    cout << "Email: " << user.email << endl;
    cout << "==========================\n" << endl;
}

void interactiveLogin(SimpleDB& db, JWT& jwt, AuthSessionManager& sessionManager) {
    cout << "\n=== User Login ===" << endl;
    
    string username, password;
    
    cout << "Username: ";
    getline(cin, username);
    
    cout << "Password: ";
    getline(cin, password);
    
    // Ищем пользователя
    auto all_users = db.getAllUsers();
    User found_user;
    
    for (const auto& user : all_users) {
        if (user.username == username) {
            found_user = user;
            break;
        }
    }
    
    if (found_user.id.empty()) {
        cout << "❌ User not found!" << endl;
        return;
    }
    
    // Проверяем пароль
    if (found_user.password_hash != hash_password(password)) {
        cout << "❌ Invalid password!" << endl;
        return;
    }
    
    // Генерируем токен
    map<string, string> payload = {
        {"user_id", found_user.id},
        {"username", found_user.username},
        {"email", found_user.email},
        {"fullname", found_user.full_name},
        {"role", found_user.role},
        {"course", found_user.course}
    };
    
    string access_token = jwt.generateToken(payload);
    string refresh_token = jwt.generateRefreshToken();
    
    // Сохраняем refresh token
    db.saveRefreshToken(found_user.id, refresh_token);
    
    cout << "✅ Login successful!" << endl;
    cout << "Access Token: " << access_token << endl;
    cout << "Refresh Token: " << refresh_token << endl;
    cout << "User ID: " << found_user.id << endl;
    cout << "==========================\n" << endl;
}

// ========== MAIN FUNCTION ==========

int main(int argc, char* argv[]) {
    cout << "========================================" << endl;
    cout << "🔐 Auth Module v3.1 (Full Registration)" << endl;
    cout << "📡 Support: GitHub OAuth + Password Auth" << endl;
    cout << "========================================" << endl;
    
    // Parse command line arguments
    bool api_mode = false;
    int api_port = 8081;
    bool useXtunnel = false;
    string xtunnelKey = "";
    string publicUrl = "";
    
    for (int i = 1; i < argc; i++) {
        string arg = argv[i];
        if (arg == "--api" || arg == "-a") {
            api_mode = true;
        } else if ((arg == "--port" || arg == "-p") && i + 1 < argc) {
            api_port = stoi(argv[++i]);
        } else if (arg == "--xtunnel" || arg == "-x") {
            useXtunnel = true;
        } else if (arg == "--xtunnel-key" && i + 1 < argc) {
            xtunnelKey = argv[++i];
            useXtunnel = true;
        } else if (arg == "--help" || arg == "-h") {
            cout << "\nUsage:" << endl;
            cout << "  " << argv[0] << "                    - Interactive mode" << endl;
            cout << "  " << argv[0] << " --api             - Start API server" << endl;
            cout << "  " << argv[0] << " --api --port 3000 - Custom port" << endl;
            cout << "  " << argv[0] << " --api --xtunnel   - With xTunnel" << endl;
            cout << "  " << argv[0] << " --help            - Show help" << endl;
            return 0;
        }
    }
    
    try {
        // Initialize core components
        Config config("config.json");
        SimpleDB db(config.getDbFile());
        
        // xTunnel integration
        if (useXtunnel) {
            cout << "\n========================================" << endl;
            cout << "🔧 XTUNNEL INTEGRATION" << endl;
            cout << "========================================" << endl;
            
            if (!XTunnelSimple::isAvailable()) {
                cout << "⚠️  xTunnel not found" << endl;
                cout << "📋 Continuing in local-only mode..." << endl;
            } else {
                cout << "✅ xTunnel found" << endl;
                
                if (XTunnelSimple::startTunnel(api_port, xtunnelKey)) {
                    publicUrl = XTunnelSimple::getTunnelUrl();
                    
                    if (!publicUrl.empty()) {
                        cout << "\n🌐 PUBLIC URL: " << publicUrl << endl;
                        cout << "📋 For team access:" << endl;
                        cout << "   API: " << publicUrl << "/api/..." << endl;
                    }
                }
            }
            cout << "========================================\n" << endl;
        }
        
        // Set GitHub redirect URI (use public URL if available)
        string githubRedirectUri;
        if (!publicUrl.empty() && publicUrl.find("https://") == 0) {
            githubRedirectUri = publicUrl + "/api/auth/callback";
        } else {
            githubRedirectUri = config.getGithubRedirectUri();
            // Ensure callback path is correct
            if (githubRedirectUri.find("/callback") == string::npos) {
                githubRedirectUri = "http://localhost:" + to_string(api_port) + "/api/auth/callback";
            }
        }
        
        cout << "🔗 GitHub OAuth redirect URI: " << githubRedirectUri << endl;
        
        GitHubOAuth github(config.getGithubClientId(),
                          config.getGithubClientSecret(),
                          githubRedirectUri);
        
        JWT jwt(config.getJwtSecret(), config.getJwtExpiryHours());
        
        db.initializeDB();
        
        cout << "✅ System initialized successfully" << endl;
        cout << "📊 Stats:" << endl;
        cout << "  • API Port: " << api_port << endl;
        cout << "  • Database: " << config.getDbFile() << " (" << db.getAllUsers().size() << " users)" << endl;
        cout << "  • xTunnel: " << (useXtunnel ? "ENABLED" : "DISABLED") << endl;
        if (!publicUrl.empty()) {
            cout << "  • Public URL: " << publicUrl << endl;
        }
        cout << "========================================\n" << endl;
        
        if (api_mode) {
            // Run in API server mode
            runFullAPIServer(api_port, jwt, db, github);
        } else {
            // Interactive mode
            printHelp();
            
            // Create session manager for interactive mode too
            AuthSessionManager sessionManager;
            
            string command;
            while (true) {
                cout << "auth> ";
                getline(cin, command);
                
                if (command == "help" || command == "?") {
                    printHelp();
                }
                else if (command == "test") {
                    testConfiguration(config, db);
                }
                else if (command == "init") {
                    // Test GitHub auth init
                    string token = sessionManager.createSession();
                    string auth_url = github.getAuthorizationUrlWithToken(token);
                    
                    cout << "\n=== Created GitHub login_token ===" << endl;
                    cout << "Token: " << token << endl;
                    cout << "GitHub URL: " << auth_url << endl;
                    cout << "===========================\n" << endl;
                }
                else if (command == "register") {
                    interactiveRegister(db, jwt, sessionManager);
                }
                else if (command == "login") {
                    interactiveLogin(db, jwt, sessionManager);
                }
                else if (command.find("status ") == 0) {
                    string token = command.substr(7);
                    if (!token.empty()) {
                        AuthSession session = sessionManager.getSession(token);
                        cout << "\n=== Session Status ===" << endl;
                        cout << "Token: " << token << endl;
                        cout << "Status: " << session.status << endl;
                        if (session.status == "success") {
                            cout << "User ID: " << session.user_id << endl;
                            cout << "Access Token: " << session.access_token.substr(0, 30) << "..." << endl;
                        }
                        cout << "=====================\n" << endl;
                    }
                }
                else if (command.find("validate ") == 0) {
                    string token = command.substr(9);
                    if (!token.empty()) {
                        auto claims = jwt.validateToken(token);
                        if (claims.empty()) {
                            cout << "❌ Token is INVALID or EXPIRED" << endl;
                        } else {
                            cout << "✅ Token is VALID" << endl;
                            cout << "📋 Claims:" << endl;
                            for (const auto& claim : claims) {
                                cout << "  " << claim.first << ": " << claim.second << endl;
                            }
                        }
                    }
                }
                else if (command == "users") {
                    auto users = db.getAllUsers();
                    cout << "\n=== Users (" << users.size() << ") ===" << endl;
                    for (const auto& user : users) {
                        cout << "• " << user.username << " (" << user.email << ")";
                        if (!user.github_id.empty()) cout << " [GitHub]";
                        cout << endl;
                    }
                    cout << "=====================\n" << endl;
                }
                else if (command == "sessions") {
                    auto sessions = sessionManager.getAllSessions();
                    cout << "\n=== Active Sessions (" << sessions.size() << ") ===" << endl;
                    for (const auto& session : sessions) {
                        cout << "• " << session.token << " [" << session.status << "]";
                        if (!session.user_id.empty()) {
                            cout << " → User: " << session.user_id;
                        }
                        cout << endl;
                    }
                    cout << "==========================\n" << endl;
                }
                else if (command == "api-start") {
                    cout << "🚀 Starting API server on port " << api_port << "..." << endl;
                    runFullAPIServer(api_port, jwt, db, github);
                    break;
                }
                else if (command == "exit" || command == "quit") {
                    cout << "👋 Goodbye!" << endl;
                    break;
                }
                else if (!command.empty()) {
                    cout << "Unknown command. Type 'help' for commands." << endl;
                }
            }
        }
        
    } catch (const exception& e) {
        cerr << "❌ Critical Error: " << e.what() << endl;
        
        // Stop xTunnel on error
        if (useXtunnel) {
            XTunnelSimple::stopTunnel();
        }
        
        return 1;
    }
    
    // Cleanup xTunnel
    if (useXtunnel) {
        XTunnelSimple::stopTunnel();
    }
    
    return 0;
}