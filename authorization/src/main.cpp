#include "../include/precompiled.h"
#include "../include/Config.h"
#include "../include/GitHubOAuth.h"
#include "../include/JWT.h"
#include "../include/SimpleDB.h"
#include "../include/TaskDB.h"
#include "../include/ZeroTierManager.h"
#include "../include/AutoSessionManager.h"

#include <iostream>
#include <iomanip>
#include <sstream>
#include <map>
#include <ctime>
#include <thread>
#include <vector>
#include <algorithm>
#include <regex>

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
        function<string(const string&, const map<string, string>&, const string&)> handler;
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
                string response_body = routes[path].handler(body, headers, path);
                
                response = "HTTP/1.1 200 OK\r\n";
                response += "Content-Type: application/json\r\n";
                response += "Access-Control-Allow-Origin: *\r\n";
                response += "Content-Length: " + to_string(response_body.length()) + "\r\n\r\n";
                response += response_body;
            } else {
                // Try to match dynamic routes (like /api/tasks/123)
                bool route_matched = false;
                for (const auto& route_pair : routes) {
                    const string& route_path = route_pair.first;
                    if (route_path.find("{") != string::npos && route_path.find("}") != string::npos) {
                        // Это динамический маршрут
                        vector<string> route_parts;
                        stringstream route_stream(route_path);
                        string part;
                        while (getline(route_stream, part, '/')) route_parts.push_back(part);
                        
                        vector<string> path_parts;
                        stringstream path_stream(path);
                        while (getline(path_stream, part, '/')) path_parts.push_back(part);
                        
                        if (route_parts.size() == path_parts.size() && 
                            routes[route_path].method == method) {
                            
                            // Извлекаем параметры из пути
                            map<string, string> path_params;
                            for (size_t i = 0; i < route_parts.size(); i++) {
                                if (route_parts[i].find("{") != string::npos) {
                                    string param_name = route_parts[i].substr(1, route_parts[i].size() - 2);
                                    path_params[param_name] = path_parts[i];
                                }
                            }
                            
                            // Передаем параметры в headers
                            for (const auto& param : path_params) {
                                headers["Path-Param-" + param.first] = param.second;
                            }
                            
                            string response_body = routes[route_path].handler(body, headers, path);
                            response = "HTTP/1.1 200 OK\r\n";
                            response += "Content-Type: application/json\r\n";
                            response += "Access-Control-Allow-Origin: *\r\n";
                            response += "Content-Length: " + to_string(response_body.length()) + "\r\n\r\n";
                            response += response_body;
                            route_matched = true;
                            break;
                        }
                    }
                }
                
                if (!route_matched) {
                    // 404
                    json error = {{"error", "Not found"}, {"path", path}};
                    string error_body = error.dump();
                    response = "HTTP/1.1 404 Not Found\r\n";
                    response += "Content-Type: application/json\r\n";
                    response += "Access-Control-Allow-Origin: *\r\n";
                    response += "Content-Length: " + to_string(error_body.length()) + "\r\n\r\n";
                    response += error_body;
                }
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
               function<string(const string&, const map<string, string>&, const string&)> handler) {
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

// ========== ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ ==========

string extract_token(const map<string, string>& headers) {
    auto it = headers.find("Authorization");
    if (it == headers.end()) return "";
    
    const string& auth = it->second;
    if (auth.find("Bearer ") == 0) {
        return auth.substr(7);
    }
    return "";
}

string get_query_param(const map<string, string>& headers, const string& param) {
    auto it = headers.find("Request-Path");
    if (it == headers.end()) return "";
    
    const string& full_path = it->second;
    size_t start = full_path.find(param + "=");
    if (start == string::npos) return "";
    
    start += param.length() + 1;
    size_t end = full_path.find("&", start);
    if (end == string::npos) end = full_path.length();
    
    string value = full_path.substr(start, end - start);
    
    // URL decode простой вариант
    string result;
    for (size_t i = 0; i < value.length(); i++) {
        if (value[i] == '%' && i + 2 < value.length()) {
            int hex = stoi(value.substr(i + 1, 2), nullptr, 16);
            result += static_cast<char>(hex);
            i += 2;
        } else if (value[i] == '+') {
            result += ' ';
        } else {
            result += value[i];
        }
    }
    
    return result;
}

string get_path_param(const map<string, string>& headers, const string& param) {
    auto key = "Path-Param-" + param;
    auto it = headers.find(key);
    if (it != headers.end()) return it->second;
    return "";
}

string hash_password(const string& password) {
    // Простой хеш для демонстрации. В продакшене используйте bcrypt/scrypt!
    hash<string> hasher;
    return to_string(hasher(password + "salt_123"));
}

// Функция для проверки email
bool is_valid_email(const string& email) {
    regex pattern(R"(^[A-Za-z0-9._%+-]+@[A-Za-z0-9.-]+\.[A-Za-z]{2,}$)");
    return regex_match(email, pattern);
}

// Функция для проверки курса
bool is_valid_course(const string& course) {
    return !course.empty() && course.find_first_not_of("123456") == string::npos;
}

// ========== TASK FLOW API ФУНКЦИИ ==========

// Проверка прав доступа к проекту
bool can_access_project(TaskDB& task_db, const string& project_id, const string& user_id) {
    return task_db.isProjectMember(project_id, user_id);
}

bool can_edit_project(TaskDB& task_db, const string& project_id, const string& user_id) {
    return task_db.isProjectOwner(project_id, user_id);
}

// Проверка прав доступа к задаче
bool can_access_task(TaskDB& task_db, const string& task_id, const string& user_id) {
    Task task = task_db.getTask(task_id);
    if (task.id.empty()) return false;
    
    return task_db.isProjectMember(task.project_id, user_id);
}

bool can_edit_task(TaskDB& task_db, const string& task_id, const string& user_id) {
    Task task = task_db.getTask(task_id);
    if (task.id.empty()) return false;
    
    return task.creator_id == user_id || task_db.isProjectOwner(task.project_id, user_id);
}

// ========== ПОЛНЫЙ API СЕРВЕР ДЛЯ TASK FLOW ==========

void runFullTaskFlowServer(int port, JWT& jwt, SimpleDB& user_db, TaskDB& task_db, GitHubOAuth& github) {
    SimpleHTTPServer server(port);
    
    // Создаем менеджер сессий для login_token
    AuthSessionManager sessionManager;
    
    // ========== BASIC ENDPOINTS ==========
    
    // 1. Health check
    server.route("GET", "/health", [](const string& body, const map<string, string>& headers, const string& path) {
        return json{
            {"status", "ok"}, 
            {"service", "task-flow-auth"}, 
            {"timestamp", time(nullptr)},
            {"version", "3.0-full"},
            {"network", "zero-tier"}
        }.dump();
    });
    
    // 2. Discovery endpoint
    server.route("GET", "/api/discovery", [](const string& body, const map<string, string>& headers, const string& path) {
        return json{
            {"service", "task_flow_auth_module"},
            {"version", "3.0-full-task-flow"},
            {"timestamp", time(nullptr)},
            {"network", "zero-tier-private"},
            {"modules", {"auth", "users", "projects", "tasks", "notifications"}},
            {"endpoints", {
                {{"method", "GET"}, {"path", "/health"}, {"description", "Health check"}},
                {{"method", "GET"}, {"path", "/api/discovery"}, {"description", "Service discovery"}},
                
                // Auth endpoints
                {{"method", "POST"}, {"path", "/api/auth/init"}, {"description", "Initialize GitHub auth session"}},
                {{"method", "GET"}, {"path", "/api/auth/callback"}, {"description", "GitHub OAuth callback"}},
                {{"method", "GET"}, {"path", "/api/auth/status"}, {"description", "Check auth status"}},
                {{"method", "POST"}, {"path", "/api/auth/validate"}, {"description", "Validate JWT token"}},
                {{"method", "POST"}, {"path", "/api/auth/refresh"}, {"description", "Refresh JWT token"}},
                {{"method", "POST"}, {"path", "/api/auth/logout"}, {"description", "Logout (revoke refresh token)"}},
                
                // User endpoints
                {{"method", "POST"}, {"path", "/api/users/register"}, {"description", "Register new user"}},
                {{"method", "POST"}, {"path", "/api/users/login"}, {"description", "Login with password"}},
                {{"method", "GET"}, {"path", "/api/users/me"}, {"description", "Get current user info"}},
                {{"method", "GET"}, {"path", "/api/users/list"}, {"description", "List all users (admin only)"}},
                {{"method", "PUT"}, {"path", "/api/users/me"}, {"description", "Update current user profile"}},
                {{"method", "PUT"}, {"path", "/api/users/me/password"}, {"description", "Change password"}},
                
                // Project endpoints
                {{"method", "POST"}, {"path", "/api/projects"}, {"description", "Create new project"}},
                {{"method", "GET"}, {"path", "/api/projects"}, {"description", "Get user's projects"}},
                {{"method", "GET"}, {"path", "/api/projects/{id}"}, {"description", "Get project by ID"}},
                {{"method", "PUT"}, {"path", "/api/projects/{id}"}, {"description", "Update project"}},
                {{"method", "DELETE"}, {"path", "/api/projects/{id}"}, {"description", "Delete project"}},
                {{"method", "POST"}, {"path", "/api/projects/{id}/members"}, {"description", "Add project member"}},
                {{"method", "DELETE"}, {"path", "/api/projects/{id}/members/{user_id}"}, {"description", "Remove project member"}},
                {{"method", "GET"}, {"path", "/api/projects/{id}/stats"}, {"description", "Get project statistics"}},
                
                // Task endpoints
                {{"method", "POST"}, {"path", "/api/tasks"}, {"description", "Create new task"}},
                {{"method", "GET"}, {"path", "/api/tasks"}, {"description", "Get user's tasks"}},
                {{"method", "GET"}, {"path", "/api/tasks/{id}"}, {"description", "Get task by ID"}},
                {{"method", "PUT"}, {"path", "/api/tasks/{id}"}, {"description", "Update task"}},
                {{"method", "DELETE"}, {"path", "/api/tasks/{id}"}, {"description", "Delete task"}},
                {{"method", "PUT"}, {"path", "/api/tasks/{id}/status"}, {"description", "Update task status"}},
                {{"method", "PUT"}, {"path", "/api/tasks/{id}/assign"}, {"description", "Assign task to user"}},
                {{"method", "GET"}, {"path", "/api/projects/{id}/tasks"}, {"description", "Get project tasks"}},
                {{"method", "GET"}, {"path", "/api/tasks/search"}, {"description", "Search tasks"}},
                
                // Comment endpoints
                {{"method", "POST"}, {"path", "/api/tasks/{id}/comments"}, {"description", "Add comment to task"}},
                {{"method", "GET"}, {"path", "/api/tasks/{id}/comments"}, {"description", "Get task comments"}},
                {{"method", "DELETE"}, {"path", "/api/comments/{id}"}, {"description", "Delete comment"}},
                
                // Notification endpoints
                {{"method", "GET"}, {"path", "/api/notifications"}, {"description", "Get user notifications"}},
                {{"method", "PUT"}, {"path", "/api/notifications/{id}/read"}, {"description", "Mark notification as read"}},
                {{"method", "PUT"}, {"path", "/api/notifications/read-all"}, {"description", "Mark all notifications as read"}},
                
                // Tag endpoints
                {{"method", "POST"}, {"path", "/api/tasks/{id}/tags"}, {"description", "Add tag to task"}},
                {{"method", "DELETE"}, {"path", "/api/tasks/{id}/tags/{tag}"}, {"description", "Remove tag from task"}},
                {{"method", "GET"}, {"path", "/api/tasks/{id}/tags"}, {"description", "Get task tags"}}
            }}
        }.dump();
    });
    
    // 3. GitHub Auth Init
    server.route("POST", "/api/auth/init", [&](const string& body, const map<string, string>& headers, const string& path) {
        try {
            string login_token = sessionManager.createSession();
            string auth_url = github.getAuthorizationUrlWithToken(login_token);
            
            json response = {
                {"success", true},
                {"token", login_token},
                {"auth_url", auth_url},
                {"expires_in", 300},
                {"timestamp", time(nullptr)}
            };
            
            cout << "[Auth] Created GitHub login_token: " << login_token << endl;
            return response.dump();
            
        } catch (const exception& e) {
            return json{{"success", false}, {"error", e.what()}}.dump();
        }
    });
    
    // 4. GitHub OAuth Callback
    server.route("GET", "/api/auth/callback", [&](const string& body, const map<string, string>& headers, const string& path) {
        string code = get_query_param(headers, "code");
        string state = get_query_param(headers, "state");
        
        cout << "[Auth] GitHub callback. Code: " << (code.empty() ? "empty" : "present")
             << ", State: " << state << endl;
        
        if (code.empty() || state.empty()) {
            return json{{"error", "Missing code or state parameters"}}.dump();
        }
        
        if (state.find("token_") != 0) {
            return json{{"error", "Invalid state format"}}.dump();
        }
        
        string login_token = state.substr(6);
        cout << "[Auth] Processing callback for token: " << login_token << endl;
        
        if (!sessionManager.updateSessionWithCode(login_token, code)) {
            return json{{"error", "Invalid or expired login_token"}}.dump();
        }
        
        // Асинхронная обработка
        thread([&, login_token, code]() {
            try {
                string github_access_token = github.getAccessToken(code);
                if (github_access_token.empty()) {
                    cerr << "[Auth] Failed to get GitHub access token" << endl;
                    sessionManager.setSessionDenied(login_token);
                    return;
                }
                
                GitHubUser github_user = github.getUserInfo(github_access_token);
                if (github_user.id.empty()) {
                    cerr << "[Auth] Failed to get GitHub user info" << endl;
                    sessionManager.setSessionDenied(login_token);
                    return;
                }
                
                cout << "[Auth] GitHub user: " << github_user.login << endl;
                
                User user = user_db.createOrUpdateUser(
                    github_user.id,
                    github_user.login,
                    github_user.email,
                    github_user.name,
                    "1",
                    ""
                );
                
                if (user.id.empty()) {
                    cerr << "[Auth] Failed to save user" << endl;
                    sessionManager.setSessionDenied(login_token);
                    return;
                }
                
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
                
                user_db.saveRefreshToken(user.id, refresh_token);
                
                bool success = sessionManager.setSessionSuccess(
                    login_token,
                    access_token,
                    refresh_token,
                    user.id,
                    github_user.id
                );
                
                if (success) {
                    cout << "[Auth] OAuth completed for user: " << user.username << endl;
                }
                     
            } catch (const exception& e) {
                cerr << "[Auth] Error processing OAuth: " << e.what() << endl;
                sessionManager.setSessionDenied(login_token);
            }
        }).detach();
        
        return json{{"status", "processing"}, {"message", "Authentication in progress. Poll /api/auth/status"}}.dump();
    });
    
    // 5. Auth Status
    server.route("GET", "/api/auth/status", [&](const string& body, const map<string, string>& headers, const string& path) {
        string token = get_query_param(headers, "token");
        
        if (token.empty()) {
            return json{{"error", "Missing token parameter"}}.dump();
        }
        
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
            sessionManager.removeSession(token);
            cout << "[Auth] Token " << token << " marked as success" << endl;
        }
        else if (session.status == "expired" || session.status == "denied") {
            sessionManager.removeSession(token);
            cout << "[Auth] Token " << token << " " << session.status << " and removed" << endl;
        }
        
        return response.dump();
    });
    
    // 6. Validate Token
    server.route("POST", "/api/auth/validate", [&](const string& body, const map<string, string>& headers, const string& path) {
        string token = extract_token(headers);
        if (token.empty()) {
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
    
    // ========== USER MANAGEMENT ==========
    
    // 7. Register User
    server.route("POST", "/api/users/register", [&](const string& body, const map<string, string>& headers, const string& path) {
        try {
            auto data = json::parse(body);
            
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
            
            if (!is_valid_email(email)) {
                return json{{"success", false}, {"error", "Invalid email format"}}.dump();
            }
            
            if (!is_valid_course(course)) {
                return json{{"success", false}, {"error", "Invalid course number (1-6)"}}.dump();
            }
            
            // Проверка существования пользователя
            auto all_users = user_db.getAllUsers();
            for (const auto& user : all_users) {
                if (user.username == username) {
                    return json{{"success", false}, {"error", "Username already exists"}}.dump();
                }
                if (user.email == email) {
                    return json{{"success", false}, {"error", "Email already registered"}}.dump();
                }
            }
            
            string password_hash = hash_password(password);
            
            cout << "[Auth] Registering new user: " << username << endl;
            
            User user = user_db.createUserWithPassword(
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
            
            // Создаем сессию
            string login_token = sessionManager.createSession();
            
            // Генерируем токены
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
            
            user_db.saveRefreshToken(user.id, refresh_token);
            
            sessionManager.setSessionSuccess(
                login_token,
                access_token,
                refresh_token,
                user.id,
                ""
            );
            
            cout << "[Auth] User registered: " << username << " (ID: " << user.id << ")" << endl;
            
            return json{
                {"success", true},
                {"message", "User registered successfully"},
                {"user_id", user.id},
                {"username", user.username},
                {"access_token", access_token},
                {"refresh_token", refresh_token}
            }.dump();
            
        } catch (const exception& e) {
            return json{{"success", false}, {"error", "Invalid request: " + string(e.what())}}.dump();
        }
    });
    
    // 8. Login
    server.route("POST", "/api/users/login", [&](const string& body, const map<string, string>& headers, const string& path) {
        try {
            auto data = json::parse(body);
            
            if (!data.contains("username") || !data.contains("password")) {
                return json{{"success", false}, {"error", "Missing username or password"}}.dump();
            }
            
            string username = data["username"];
            string password = data["password"];
            
            // Ищем пользователя
            auto all_users = user_db.getAllUsers();
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
            
            if (found_user.password_hash != hash_password(password)) {
                return json{{"success", false}, {"error", "Invalid password"}}.dump();
            }
            
            cout << "[Auth] User login successful: " << username << endl;
            
            string login_token = sessionManager.createSession();
            
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
            
            user_db.saveRefreshToken(found_user.id, refresh_token);
            
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
                {"user_id", found_user.id},
                {"username", found_user.username}
            }.dump();
            
        } catch (const exception& e) {
            return json{{"success", false}, {"error", "Invalid request: " + string(e.what())}}.dump();
        }
    });
    
    // 9. Get Current User
    server.route("GET", "/api/users/me", [&](const string& body, const map<string, string>& headers, const string& path) {
        string token = extract_token(headers);
        if (token.empty()) {
            return json{{"success", false}, {"error", "No token provided"}}.dump();
        }
        
        auto claims = jwt.validateToken(token);
        if (claims.empty()) {
            return json{{"success", false}, {"error", "Invalid token"}}.dump();
        }
        
        User user = user_db.getUserById(claims["user_id"]);
        if (user.id.empty()) {
            return json{{"success", false}, {"error", "User not found"}}.dump();
        }
        
        // Получаем статистику пользователя из TaskDB
        auto user_stats = task_db.getUserStats(user.id);
        auto user_notifications = task_db.getUserNotifications(user.id, true);
        
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
            }},
            {"stats", user_stats},
            {"unread_notifications", user_notifications.size()}
        }.dump();
    });
    
    // 10. List All Users (Admin only)
    server.route("GET", "/api/users/list", [&](const string& body, const map<string, string>& headers, const string& path) {
        string token = extract_token(headers);
        auto claims = jwt.validateToken(token);
        
        if (claims.empty()) {
            return json{{"success", false}, {"error", "Invalid token"}}.dump();
        }
        if (claims.find("role") == claims.end() || claims["role"] != "admin") {
            return json{{"success", false}, {"error", "Admin access required"}}.dump();
        }
        
        auto users = user_db.getAllUsers();
        json users_json = json::array();
        
        for (const auto& user : users) {
            auto user_stats = task_db.getUserStats(user.id);
            
            users_json.push_back({
                {"id", user.id},
                {"username", user.username},
                {"email", user.email},
                {"full_name", user.full_name},
                {"role", user.role},
                {"course", user.course},
                {"has_github", !user.github_id.empty()},
                {"stats", user_stats}
            });
        }
        
        return json{{"success", true}, {"users", users_json}, {"count", users.size()}}.dump();
    });
    
    // ========== PROJECT MANAGEMENT ==========
    
    // 11. Create Project
    server.route("POST", "/api/projects", [&](const string& body, const map<string, string>& headers, const string& path) {
        string token = extract_token(headers);
        auto claims = jwt.validateToken(token);
        
        if (claims.empty()) {
            return json{{"success", false}, {"error", "Authentication required"}}.dump();
        }
        
        try {
            auto data = json::parse(body);
            
            if (!data.contains("name") || !data.contains("description")) {
                return json{{"success", false}, {"error", "Missing name or description"}}.dump();
            }
            
            string name = data["name"];
            string description = data["description"];
            
            if (name.empty() || description.empty()) {
                return json{{"success", false}, {"error", "Name and description cannot be empty"}}.dump();
            }
            
            Project project = task_db.createProject(name, description, claims["user_id"]);
            
            if (project.id.empty()) {
                return json{{"success", false}, {"error", "Failed to create project"}}.dump();
            }
            
            cout << "[TaskFlow] Created project: " << name << " by user " << claims["user_id"] << endl;
            
            return json{
                {"success", true},
                {"message", "Project created successfully"},
                {"project", {
                    {"id", project.id},
                    {"name", project.name},
                    {"description", project.description},
                    {"owner_id", project.owner_id},
                    {"created_at", project.created_at}
                }}
            }.dump();
            
        } catch (const exception& e) {
            return json{{"success", false}, {"error", "Invalid request: " + string(e.what())}}.dump();
        }
    });
    
    // 12. Get User's Projects
    server.route("GET", "/api/projects", [&](const string& body, const map<string, string>& headers, const string& path) {
        string token = extract_token(headers);
        auto claims = jwt.validateToken(token);
        
        if (claims.empty()) {
            return json{{"success", false}, {"error", "Authentication required"}}.dump();
        }
        
        auto projects = task_db.getUserProjects(claims["user_id"]);
        json projects_json = json::array();
        
        for (const auto& project : projects) {
            auto stats = task_db.getProjectStats(project.id);
            
            projects_json.push_back({
                {"id", project.id},
                {"name", project.name},
                {"description", project.description},
                {"owner_id", project.owner_id},
                {"member_count", project.member_ids.size()},
                {"created_at", project.created_at},
                {"updated_at", project.updated_at},
                {"stats", stats}
            });
        }
        
        return json{
            {"success", true},
            {"projects", projects_json},
            {"count", projects.size()}
        }.dump();
    });
    
    // 13. Get Project by ID
    server.route("GET", "/api/projects/{id}", [&](const string& body, const map<string, string>& headers, const string& path) {
        string token = extract_token(headers);
        auto claims = jwt.validateToken(token);
        
        if (claims.empty()) {
            return json{{"success", false}, {"error", "Authentication required"}}.dump();
        }
        
        string project_id = get_path_param(headers, "id");
        if (project_id.empty()) {
            return json{{"success", false}, {"error", "Project ID required"}}.dump();
        }
        
        if (!can_access_project(task_db, project_id, claims["user_id"])) {
            return json{{"success", false}, {"error", "Access denied to project"}}.dump();
        }
        
        Project project = task_db.getProject(project_id);
        if (project.id.empty()) {
            return json{{"success", false}, {"error", "Project not found"}}.dump();
        }
        
        auto stats = task_db.getProjectStats(project.id);
        
        // Получаем информацию о владельце
        User owner = user_db.getUserById(project.owner_id);
        
        return json{
            {"success", true},
            {"project", {
                {"id", project.id},
                {"name", project.name},
                {"description", project.description},
                {"owner", {
                    {"id", owner.id},
                    {"username", owner.username},
                    {"full_name", owner.full_name}
                }},
                {"member_count", project.member_ids.size()},
                {"created_at", project.created_at},
                {"updated_at", project.updated_at},
                {"stats", stats}
            }}
        }.dump();
    });
    
    // 14. Update Project
    server.route("PUT", "/api/projects/{id}", [&](const string& body, const map<string, string>& headers, const string& path) {
        string token = extract_token(headers);
        auto claims = jwt.validateToken(token);
        
        if (claims.empty()) {
            return json{{"success", false}, {"error", "Authentication required"}}.dump();
        }
        
        string project_id = get_path_param(headers, "id");
        if (project_id.empty()) {
            return json{{"success", false}, {"error", "Project ID required"}}.dump();
        }
        
        if (!can_edit_project(task_db, project_id, claims["user_id"])) {
            return json{{"success", false}, {"error", "Only project owner can edit project"}}.dump();
        }
        
        try {
            auto data = json::parse(body);
            map<string, string> updates;
            
            if (data.contains("name")) updates["name"] = data["name"];
            if (data.contains("description")) updates["description"] = data["description"];
            
            if (updates.empty()) {
                return json{{"success", false}, {"error", "No updates provided"}}.dump();
            }
            
            if (task_db.updateProject(project_id, updates)) {
                return json{{"success", true}, {"message", "Project updated successfully"}}.dump();
            } else {
                return json{{"success", false}, {"error", "Failed to update project"}}.dump();
            }
            
        } catch (const exception& e) {
            return json{{"success", false}, {"error", "Invalid request: " + string(e.what())}}.dump();
        }
    });
    
    // 15. Delete Project
    server.route("DELETE", "/api/projects/{id}", [&](const string& body, const map<string, string>& headers, const string& path) {
        string token = extract_token(headers);
        auto claims = jwt.validateToken(token);
        
        if (claims.empty()) {
            return json{{"success", false}, {"error", "Authentication required"}}.dump();
        }
        
        string project_id = get_path_param(headers, "id");
        if (project_id.empty()) {
            return json{{"success", false}, {"error", "Project ID required"}}.dump();
        }
        
        if (!can_edit_project(task_db, project_id, claims["user_id"])) {
            return json{{"success", false}, {"error", "Only project owner can delete project"}}.dump();
        }
        
        if (task_db.deleteProject(project_id)) {
            cout << "[TaskFlow] Deleted project: " << project_id << " by user " << claims["user_id"] << endl;
            return json{{"success", true}, {"message", "Project deleted successfully"}}.dump();
        } else {
            return json{{"success", false}, {"error", "Failed to delete project"}}.dump();
        }
    });
    
    // 16. Add Project Member
    server.route("POST", "/api/projects/{id}/members", [&](const string& body, const map<string, string>& headers, const string& path) {
        string token = extract_token(headers);
        auto claims = jwt.validateToken(token);
        
        if (claims.empty()) {
            return json{{"success", false}, {"error", "Authentication required"}}.dump();
        }
        
        string project_id = get_path_param(headers, "id");
        if (project_id.empty()) {
            return json{{"success", false}, {"error", "Project ID required"}}.dump();
        }
        
        if (!can_edit_project(task_db, project_id, claims["user_id"])) {
            return json{{"success", false}, {"error", "Only project owner can add members"}}.dump();
        }
        
        try {
            auto data = json::parse(body);
            
            if (!data.contains("user_id")) {
                return json{{"success", false}, {"error", "User ID required"}}.dump();
            }
            
            string user_id = data["user_id"];
            
            // Проверяем существование пользователя
            User user = user_db.getUserById(user_id);
            if (user.id.empty()) {
                return json{{"success", false}, {"error", "User not found"}}.dump();
            }
            
            if (task_db.addProjectMember(project_id, user_id)) {
                cout << "[TaskFlow] Added user " << user_id << " to project " << project_id << endl;
                return json{
                    {"success", true},
                    {"message", "Member added successfully"},
                    {"user", {
                        {"id", user.id},
                        {"username", user.username},
                        {"full_name", user.full_name}
                    }}
                }.dump();
            } else {
                return json{{"success", false}, {"error", "Failed to add member"}}.dump();
            }
            
        } catch (const exception& e) {
            return json{{"success", false}, {"error", "Invalid request: " + string(e.what())}}.dump();
        }
    });
    
    // 17. Remove Project Member
    server.route("DELETE", "/api/projects/{id}/members/{user_id}", [&](const string& body, const map<string, string>& headers, const string& path) {
        string token = extract_token(headers);
        auto claims = jwt.validateToken(token);
        
        if (claims.empty()) {
            return json{{"success", false}, {"error", "Authentication required"}}.dump();
        }
        
        string project_id = get_path_param(headers, "id");
        string member_id = get_path_param(headers, "user_id");
        
        if (project_id.empty() || member_id.empty()) {
            return json{{"success", false}, {"error", "Project ID and User ID required"}}.dump();
        }
        
        if (!can_edit_project(task_db, project_id, claims["user_id"])) {
            return json{{"success", false}, {"error", "Only project owner can remove members"}}.dump();
        }
        
        if (task_db.removeProjectMember(project_id, member_id)) {
            cout << "[TaskFlow] Removed user " << member_id << " from project " << project_id << endl;
            return json{{"success", true}, {"message", "Member removed successfully"}}.dump();
        } else {
            return json{{"success", false}, {"error", "Failed to remove member"}}.dump();
        }
    });
    
    // 18. Get Project Statistics
    server.route("GET", "/api/projects/{id}/stats", [&](const string& body, const map<string, string>& headers, const string& path) {
        string token = extract_token(headers);
        auto claims = jwt.validateToken(token);
        
        if (claims.empty()) {
            return json{{"success", false}, {"error", "Authentication required"}}.dump();
        }
        
        string project_id = get_path_param(headers, "id");
        if (project_id.empty()) {
            return json{{"success", false}, {"error", "Project ID required"}}.dump();
        }
        
        if (!can_access_project(task_db, project_id, claims["user_id"])) {
            return json{{"success", false}, {"error", "Access denied to project"}}.dump();
        }
        
        auto stats = task_db.getProjectStats(project_id);
        auto tasks = task_db.getProjectTasks(project_id);
        
        // Статистика по пользователям
        map<string, int> user_task_counts;
        for (const auto& task : tasks) {
            if (!task.assignee_id.empty()) {
                user_task_counts[task.assignee_id]++;
            }
        }
        
        json user_stats = json::array();
        for (const auto& pair : user_task_counts) {
            User user = user_db.getUserById(pair.first);
            if (!user.id.empty()) {
                user_stats.push_back({
                    {"user_id", user.id},
                    {"username", user.username},
                    {"task_count", pair.second}
                });
            }
        }
        
        return json{
            {"success", true},
            {"project_id", project_id},
            {"task_statistics", stats},
            {"user_statistics", user_stats},
            {"total_tasks", tasks.size()}
        }.dump();
    });
    
    // ========== TASK MANAGEMENT ==========
    
    // 19. Create Task
    server.route("POST", "/api/tasks", [&](const string& body, const map<string, string>& headers, const string& path) {
        string token = extract_token(headers);
        auto claims = jwt.validateToken(token);
        
        if (claims.empty()) {
            return json{{"success", false}, {"error", "Authentication required"}}.dump();
        }
        
        try {
            auto data = json::parse(body);
            
            if (!data.contains("title") || !data.contains("description") || !data.contains("project_id")) {
                return json{{"success", false}, {"error", "Missing required fields"}}.dump();
            }
            
            string title = data["title"];
            string description = data["description"];
            string project_id = data["project_id"];
            string assignee_id = data.value("assignee_id", "");
            
            if (title.empty() || description.empty() || project_id.empty()) {
                return json{{"success", false}, {"error", "Fields cannot be empty"}}.dump();
            }
            
            if (!can_access_project(task_db, project_id, claims["user_id"])) {
                return json{{"success", false}, {"error", "Access denied to project"}}.dump();
            }
            
            // Проверяем исполнителя, если указан
            if (!assignee_id.empty() && !task_db.isProjectMember(project_id, assignee_id)) {
                return json{{"success", false}, {"error", "Assignee must be a project member"}}.dump();
            }
            
            // Парсим due_date если есть
            time_t due_date = 0;
            if (data.contains("due_date") && !data["due_date"].is_null()) {
                string due_date_str = data["due_date"];
                if (!due_date_str.empty()) {
                    istringstream ss(due_date_str);
                    tm timeinfo = {};
                    ss >> get_time(&timeinfo, "%Y-%m-%d");
                    if (!ss.fail()) {
                        due_date = mktime(&timeinfo);
                    } else {
                        cerr << "Warning: Invalid date format: " << due_date_str << endl;
                    }
                }
            }
            
            Task task = task_db.createTask(
                title,
                description,
                project_id,
                claims["user_id"],
                assignee_id,
                due_date
            );
            
            if (task.id.empty()) {
                return json{{"success", false}, {"error", "Failed to create task"}}.dump();
            }
            
            cout << "[TaskFlow] Created task: " << title << " in project " << project_id << endl;
            
            return json{
                {"success", true},
                {"message", "Task created successfully"},
                {"task", {
                    {"id", task.id},
                    {"title", task.title},
                    {"description", task.description},
                    {"status", task.status},
                    {"project_id", task.project_id},
                    {"assignee_id", task.assignee_id},
                    {"creator_id", task.creator_id},
                    {"created_at", task.created_at},
                    {"due_date", task.due_date}
                }}
            }.dump();
            
        } catch (const exception& e) {
            return json{{"success", false}, {"error", "Invalid request: " + string(e.what())}}.dump();
        }
    });
    
    // 20. Get User's Tasks
    server.route("GET", "/api/tasks", [&](const string& body, const map<string, string>& headers, const string& path) {
        string token = extract_token(headers);
        auto claims = jwt.validateToken(token);
        
        if (claims.empty()) {
            return json{{"success", false}, {"error", "Authentication required"}}.dump();
        }
        
        // Фильтры из query параметров
        string project_id = get_query_param(headers, "project_id");
        string status = get_query_param(headers, "status");
        string assigned = get_query_param(headers, "assigned");
        
        vector<Task> tasks;
        
        if (assigned == "true") {
            tasks = task_db.getUserAssignedTasks(claims["user_id"]);
        } else {
            tasks = task_db.getUserTasks(claims["user_id"]);
        }
        
        // Применяем фильтры
        vector<Task> filtered_tasks;
        for (const auto& task : tasks) {
            if (!project_id.empty() && task.project_id != project_id) {
                continue;
            }
            if (!status.empty() && task.status != status) {
                continue;
            }
            filtered_tasks.push_back(task);
        }
        
        json tasks_json = json::array();
        for (const auto& task : filtered_tasks) {
            // Получаем информацию о проекте
            Project project = task_db.getProject(task.project_id);
            // Получаем информацию об исполнителе
            User assignee = user_db.getUserById(task.assignee_id);
            User creator = user_db.getUserById(task.creator_id);
            
            tasks_json.push_back({
                {"id", task.id},
                {"title", task.title},
                {"description", task.description},
                {"status", task.status},
                {"project", {
                    {"id", project.id},
                    {"name", project.name}
                }},
                {"assignee", assignee.id.empty() ? json() : json{
                    {"id", assignee.id},
                    {"username", assignee.username}
                }},
                {"creator", {
                    {"id", creator.id},
                    {"username", creator.username}
                }},
                {"tags", task.tags},
                {"created_at", task.created_at},
                {"due_date", task.due_date},
                {"completed_at", task.completed_at}
            });
        }
        
        return json{
            {"success", true},
            {"tasks", tasks_json},
            {"count", filtered_tasks.size()}
        }.dump();
    });
    
    // 21. Get Task by ID
    server.route("GET", "/api/tasks/{id}", [&](const string& body, const map<string, string>& headers, const string& path) {
        string token = extract_token(headers);
        auto claims = jwt.validateToken(token);
        
        if (claims.empty()) {
            return json{{"success", false}, {"error", "Authentication required"}}.dump();
        }
        
        string task_id = get_path_param(headers, "id");
        if (task_id.empty()) {
            return json{{"success", false}, {"error", "Task ID required"}}.dump();
        }
        
        if (!can_access_task(task_db, task_id, claims["user_id"])) {
            return json{{"success", false}, {"error", "Access denied to task"}}.dump();
        }
        
        Task task = task_db.getTask(task_id);
        if (task.id.empty()) {
            return json{{"success", false}, {"error", "Task not found"}}.dump();
        }
        
        // Получаем связанную информацию
        Project project = task_db.getProject(task.project_id);
        User assignee = user_db.getUserById(task.assignee_id);
        User creator = user_db.getUserById(task.creator_id);
        auto comments = task_db.getTaskComments(task.id);
        auto tags = task_db.getTaskTags(task.id);
        
        json comments_json = json::array();
        for (const auto& comment : comments) {
            User comment_user = user_db.getUserById(comment.user_id);
            comments_json.push_back({
                {"id", comment.id},
                {"content", comment.content},
                {"user", {
                    {"id", comment_user.id},
                    {"username", comment_user.username},
                    {"full_name", comment_user.full_name}
                }},
                {"created_at", comment.created_at}
            });
        }
        
        return json{
            {"success", true},
            {"task", {
                {"id", task.id},
                {"title", task.title},
                {"description", task.description},
                {"status", task.status},
                {"project", {
                    {"id", project.id},
                    {"name", project.name}
                }},
                {"assignee", assignee.id.empty() ? json() : json{
                    {"id", assignee.id},
                    {"username", assignee.username},
                    {"full_name", assignee.full_name}
                }},
                {"creator", {
                    {"id", creator.id},
                    {"username", creator.username},
                    {"full_name", creator.full_name}
                }},
                {"tags", tags},
                {"comments", comments_json},
                {"comment_count", comments.size()},
                {"created_at", task.created_at},
                {"due_date", task.due_date},
                {"completed_at", task.completed_at}
            }}
        }.dump();
    });
    
    // 22. Update Task
    server.route("PUT", "/api/tasks/{id}", [&](const string& body, const map<string, string>& headers, const string& path) {
        string token = extract_token(headers);
        auto claims = jwt.validateToken(token);
        
        if (claims.empty()) {
            return json{{"success", false}, {"error", "Authentication required"}}.dump();
        }
        
        string task_id = get_path_param(headers, "id");
        if (task_id.empty()) {
            return json{{"success", false}, {"error", "Task ID required"}}.dump();
        }
        
        if (!can_edit_task(task_db, task_id, claims["user_id"])) {
            return json{{"success", false}, {"error", "Permission denied to edit task"}}.dump();
        }
        
        try {
            auto data = json::parse(body);
            map<string, string> updates;
            
            if (data.contains("title")) updates["title"] = data["title"];
            if (data.contains("description")) updates["description"] = data["description"];
            if (data.contains("status")) updates["status"] = data["status"];
            if (data.contains("assignee_id")) updates["assignee_id"] = data["assignee_id"];
            
            if (updates.empty()) {
                return json{{"success", false}, {"error", "No updates provided"}}.dump();
            }
            
            // Проверяем исполнителя, если обновляется
            if (updates.find("assignee_id") != updates.end() && !updates["assignee_id"].empty()) {
                Task task = task_db.getTask(task_id);
                if (!task_db.isProjectMember(task.project_id, updates["assignee_id"])) {
                    return json{{"success", false}, {"error", "Assignee must be a project member"}}.dump();
                }
            }
            
            if (task_db.updateTask(task_id, updates)) {
                cout << "[TaskFlow] Updated task: " << task_id << endl;
                return json{{"success", true}, {"message", "Task updated successfully"}}.dump();
            } else {
                return json{{"success", false}, {"error", "Failed to update task"}}.dump();
            }
            
        } catch (const exception& e) {
            return json{{"success", false}, {"error", "Invalid request: " + string(e.what())}}.dump();
        }
    });
    
    // 23. Delete Task
    server.route("DELETE", "/api/tasks/{id}", [&](const string& body, const map<string, string>& headers, const string& path) {
        string token = extract_token(headers);
        auto claims = jwt.validateToken(token);
        
        if (claims.empty()) {
            return json{{"success", false}, {"error", "Authentication required"}}.dump();
        }
        
        string task_id = get_path_param(headers, "id");
        if (task_id.empty()) {
            return json{{"success", false}, {"error", "Task ID required"}}.dump();
        }
        
        if (!can_edit_task(task_db, task_id, claims["user_id"])) {
            return json{{"success", false}, {"error", "Permission denied to delete task"}}.dump();
        }
        
        if (task_db.deleteTask(task_id)) {
            cout << "[TaskFlow] Deleted task: " << task_id << endl;
            return json{{"success", true}, {"message", "Task deleted successfully"}}.dump();
        } else {
            return json{{"success", false}, {"error", "Failed to delete task"}}.dump();
        }
    });
    
    // 24. Update Task Status
    server.route("PUT", "/api/tasks/{id}/status", [&](const string& body, const map<string, string>& headers, const string& path) {
        string token = extract_token(headers);
        auto claims = jwt.validateToken(token);
        
        if (claims.empty()) {
            return json{{"success", false}, {"error", "Authentication required"}}.dump();
        }
        
        string task_id = get_path_param(headers, "id");
        if (task_id.empty()) {
            return json{{"success", false}, {"error", "Task ID required"}}.dump();
        }
        
        Task task = task_db.getTask(task_id);
        if (task.id.empty()) {
            return json{{"success", false}, {"error", "Task not found"}}.dump();
        }
        
        // Проверяем доступ: исполнитель или создатель или владелец проекта
        bool can_change = false;
        if (task.assignee_id == claims["user_id"]) can_change = true;
        if (task.creator_id == claims["user_id"]) can_change = true;
        if (task_db.isProjectOwner(task.project_id, claims["user_id"])) can_change = true;
        
        if (!can_change) {
            return json{{"success", false}, {"error", "Permission denied to change task status"}}.dump();
        }
        
        try {
            auto data = json::parse(body);
            
            if (!data.contains("status")) {
                return json{{"success", false}, {"error", "Status required"}}.dump();
            }
            
            string status = data["status"];
            vector<string> valid_statuses = {"todo", "in_progress", "review", "done"};
            
            if (find(valid_statuses.begin(), valid_statuses.end(), status) == valid_statuses.end()) {
                return json{{"success", false}, {"error", "Invalid status"}}.dump();
            }
            
            if (task_db.updateTaskStatus(task_id, status)) {
                cout << "[TaskFlow] Updated task " << task_id << " status to " << status << endl;
                return json{{"success", true}, {"message", "Task status updated successfully"}}.dump();
            } else {
                return json{{"success", false}, {"error", "Failed to update task status"}}.dump();
            }
            
        } catch (const exception& e) {
            return json{{"success", false}, {"error", "Invalid request: " + string(e.what())}}.dump();
        }
    });
    
    // 25. Assign Task
    server.route("PUT", "/api/tasks/{id}/assign", [&](const string& body, const map<string, string>& headers, const string& path) {
        string token = extract_token(headers);
        auto claims = jwt.validateToken(token);
        
        if (claims.empty()) {
            return json{{"success", false}, {"error", "Authentication required"}}.dump();
        }
        
        string task_id = get_path_param(headers, "id");
        if (task_id.empty()) {
            return json{{"success", false}, {"error", "Task ID required"}}.dump();
        }
        
        Task task = task_db.getTask(task_id);
        if (task.id.empty()) {
            return json{{"success", false}, {"error", "Task not found"}}.dump();
        }
        
        // Проверяем права: создатель или владелец проекта
        if (task.creator_id != claims["user_id"] && 
            !task_db.isProjectOwner(task.project_id, claims["user_id"])) {
            return json{{"success", false}, {"error", "Permission denied to assign task"}}.dump();
        }
        
        try {
            auto data = json::parse(body);
            
            if (!data.contains("user_id")) {
                return json{{"success", false}, {"error", "User ID required"}}.dump();
            }
            
            string user_id = data["user_id"];
            
            // Проверяем что пользователь существует и является участником проекта
            User user = user_db.getUserById(user_id);
            if (user.id.empty()) {
                return json{{"success", false}, {"error", "User not found"}}.dump();
            }
            
            if (!task_db.isProjectMember(task.project_id, user_id)) {
                return json{{"success", false}, {"error", "User is not a project member"}}.dump();
            }
            
            if (task_db.assignTask(task_id, user_id)) {
                cout << "[TaskFlow] Assigned task " << task_id << " to user " << user_id << endl;
                return json{
                    {"success", true},
                    {"message", "Task assigned successfully"},
                    {"task_id", task_id},
                    {"assignee", {
                        {"id", user.id},
                        {"username", user.username},
                        {"full_name", user.full_name}
                    }}
                }.dump();
            } else {
                return json{{"success", false}, {"error", "Failed to assign task"}}.dump();
            }
            
        } catch (const exception& e) {
            return json{{"success", false}, {"error", "Invalid request: " + string(e.what())}}.dump();
        }
    });
    
    // 26. Get Project Tasks
    server.route("GET", "/api/projects/{id}/tasks", [&](const string& body, const map<string, string>& headers, const string& path) {
        string token = extract_token(headers);
        auto claims = jwt.validateToken(token);
        
        if (claims.empty()) {
            return json{{"success", false}, {"error", "Authentication required"}}.dump();
        }
        
        string project_id = get_path_param(headers, "id");
        if (project_id.empty()) {
            return json{{"success", false}, {"error", "Project ID required"}}.dump();
        }
        
        if (!can_access_project(task_db, project_id, claims["user_id"])) {
            return json{{"success", false}, {"error", "Access denied to project"}}.dump();
        }
        
        auto tasks = task_db.getProjectTasks(project_id);
        json tasks_json = json::array();
        
        for (const auto& task : tasks) {
            User assignee = user_db.getUserById(task.assignee_id);
            User creator = user_db.getUserById(task.creator_id);
            
            tasks_json.push_back({
                {"id", task.id},
                {"title", task.title},
                {"description", task.description},
                {"status", task.status},
                {"assignee", assignee.id.empty() ? json() : json{
                    {"id", assignee.id},
                    {"username", assignee.username}
                }},
                {"creator", {
                    {"id", creator.id},
                    {"username", creator.username}
                }},
                {"tags", task.tags},
                {"created_at", task.created_at},
                {"due_date", task.due_date},
                {"completed_at", task.completed_at}
            });
        }
        
        return json{
            {"success", true},
            {"project_id", project_id},
            {"tasks", tasks_json},
            {"count", tasks.size()}
        }.dump();
    });
    
    // 27. Search Tasks
    server.route("GET", "/api/tasks/search", [&](const string& body, const map<string, string>& headers, const string& path) {
        string token = extract_token(headers);
        auto claims = jwt.validateToken(token);
        
        if (claims.empty()) {
            return json{{"success", false}, {"error", "Authentication required"}}.dump();
        }
        
        string query = get_query_param(headers, "q");
        string project_id = get_query_param(headers, "project_id");
        string status = get_query_param(headers, "status");
        string assignee_id = get_query_param(headers, "assignee_id");
        string tag = get_query_param(headers, "tag");
        
        vector<Task> results;
        
        // Поиск по тегу
        if (!tag.empty() && !project_id.empty()) {
            results = task_db.getTasksByTag(project_id, tag);
        } else {
            // Обычный поиск
            results = task_db.searchTasks(project_id, query, status, assignee_id);
        }
        
        // Фильтруем только те задачи, к которым есть доступ
        vector<Task> accessible_results;
        for (const auto& task : results) {
            if (can_access_task(task_db, task.id, claims["user_id"])) {
                accessible_results.push_back(task);
            }
        }
        
        json tasks_json = json::array();
        for (const auto& task : accessible_results) {
            Project project = task_db.getProject(task.project_id);
            User assignee = user_db.getUserById(task.assignee_id);
            
            tasks_json.push_back({
                {"id", task.id},
                {"title", task.title},
                {"description", task.description.length() > 100 ? 
                    task.description.substr(0, 100) + "..." : task.description},
                {"status", task.status},
                {"project", {
                    {"id", project.id},
                    {"name", project.name}
                }},
                {"assignee", assignee.id.empty() ? json() : json{
                    {"id", assignee.id},
                    {"username", assignee.username}
                }},
                {"tags", task.tags},
                {"created_at", task.created_at},
                {"due_date", task.due_date}
            });
        }
        
        return json{
            {"success", true},
            {"query", query},
            {"results", tasks_json},
            {"count", accessible_results.size()}
        }.dump();
    });
    
    // ========== COMMENT MANAGEMENT ==========
    
    // 28. Add Comment
    server.route("POST", "/api/tasks/{id}/comments", [&](const string& body, const map<string, string>& headers, const string& path) {
        string token = extract_token(headers);
        auto claims = jwt.validateToken(token);
        
        if (claims.empty()) {
            return json{{"success", false}, {"error", "Authentication required"}}.dump();
        }
        
        string task_id = get_path_param(headers, "id");
        if (task_id.empty()) {
            return json{{"success", false}, {"error", "Task ID required"}}.dump();
        }
        
        if (!can_access_task(task_db, task_id, claims["user_id"])) {
            return json{{"success", false}, {"error", "Access denied to task"}}.dump();
        }
        
        try {
            auto data = json::parse(body);
            
            if (!data.contains("content") || data["content"].empty()) {
                return json{{"success", false}, {"error", "Comment content required"}}.dump();
            }
            
            string content = data["content"];
            
            Comment comment = task_db.addComment(task_id, claims["user_id"], content);
            
            if (comment.id.empty()) {
                return json{{"success", false}, {"error", "Failed to add comment"}}.dump();
            }
            
            User comment_user = user_db.getUserById(claims["user_id"]);
            
            cout << "[TaskFlow] Added comment to task " << task_id << endl;
            
            return json{
                {"success", true},
                {"message", "Comment added successfully"},
                {"comment", {
                    {"id", comment.id},
                    {"content", comment.content},
                    {"user", {
                        {"id", comment_user.id},
                        {"username", comment_user.username},
                        {"full_name", comment_user.full_name}
                    }},
                    {"created_at", comment.created_at}
                }}
            }.dump();
            
        } catch (const exception& e) {
            return json{{"success", false}, {"error", "Invalid request: " + string(e.what())}}.dump();
        }
    });
    
    // 29. Get Task Comments
    server.route("GET", "/api/tasks/{id}/comments", [&](const string& body, const map<string, string>& headers, const string& path) {
        string token = extract_token(headers);
        auto claims = jwt.validateToken(token);
        
        if (claims.empty()) {
            return json{{"success", false}, {"error", "Authentication required"}}.dump();
        }
        
        string task_id = get_path_param(headers, "id");
        if (task_id.empty()) {
            return json{{"success", false}, {"error", "Task ID required"}}.dump();
        }
        
        if (!can_access_task(task_db, task_id, claims["user_id"])) {
            return json{{"success", false}, {"error", "Access denied to task"}}.dump();
        }
        
        auto comments = task_db.getTaskComments(task_id);
        json comments_json = json::array();
        
        for (const auto& comment : comments) {
            User comment_user = user_db.getUserById(comment.user_id);
            
            comments_json.push_back({
                {"id", comment.id},
                {"content", comment.content},
                {"user", {
                    {"id", comment_user.id},
                    {"username", comment_user.username},
                    {"full_name", comment_user.full_name}
                }},
                {"created_at", comment.created_at}
            });
        }
        
        return json{
            {"success", true},
            {"task_id", task_id},
            {"comments", comments_json},
            {"count", comments.size()}
        }.dump();
    });
    
    // 30. Delete Comment
    server.route("DELETE", "/api/comments/{id}", [&](const string& body, const map<string, string>& headers, const string& path) {
        string token = extract_token(headers);
        auto claims = jwt.validateToken(token);
        
        if (claims.empty()) {
            return json{{"success", false}, {"error", "Authentication required"}}.dump();
        }
        
        string comment_id = get_path_param(headers, "id");
        if (comment_id.empty()) {
            return json{{"success", false}, {"error", "Comment ID required"}}.dump();
        }
        
        // Нужно получить комментарий, чтобы проверить права
        // (в реальной реализации нужно добавить метод getComment в TaskDB)
        // Пока разрешаем удалять только админам
        User user = user_db.getUserById(claims["user_id"]);
        if (user.role != "admin") {
            return json{{"success", false}, {"error", "Only admins can delete comments"}}.dump();
        }
        
        if (task_db.deleteComment(comment_id)) {
            cout << "[TaskFlow] Deleted comment: " << comment_id << endl;
            return json{{"success", true}, {"message", "Comment deleted successfully"}}.dump();
        } else {
            return json{{"success", false}, {"error", "Failed to delete comment"}}.dump();
        }
    });
    
    // ========== NOTIFICATION MANAGEMENT ==========
    
    // 31. Get User Notifications
    server.route("GET", "/api/notifications", [&](const string& body, const map<string, string>& headers, const string& path) {
        string token = extract_token(headers);
        auto claims = jwt.validateToken(token);
        
        if (claims.empty()) {
            return json{{"success", false}, {"error", "Authentication required"}}.dump();
        }
        
        string unread_only_str = get_query_param(headers, "unread_only");
        bool unread_only = (unread_only_str == "true");
        
        auto notifications = task_db.getUserNotifications(claims["user_id"], unread_only);
        json notifications_json = json::array();
        
        for (const auto& notification : notifications) {
            notifications_json.push_back({
                {"id", notification.id},
                {"type", notification.type},
                {"message", notification.message},
                {"entity_id", notification.entity_id},
                {"read", notification.read},
                {"created_at", notification.created_at}
            });
        }
        
        return json{
            {"success", true},
            {"notifications", notifications_json},
            {"count", notifications.size()},
            {"unread_count", task_db.getUserNotifications(claims["user_id"], true).size()}
        }.dump();
    });
    
    // 32. Mark Notification as Read
    server.route("PUT", "/api/notifications/{id}/read", [&](const string& body, const map<string, string>& headers, const string& path) {
        string token = extract_token(headers);
        auto claims = jwt.validateToken(token);
        
        if (claims.empty()) {
            return json{{"success", false}, {"error", "Authentication required"}}.dump();
        }
        
        string notification_id = get_path_param(headers, "id");
        if (notification_id.empty()) {
            return json{{"success", false}, {"error", "Notification ID required"}}.dump();
        }
        
        // Нужно проверить, что уведомление принадлежит пользователю
        // (в реальной реализации нужно добавить метод getNotification в TaskDB)
        // Пока просто отмечаем как прочитанное
        if (task_db.markNotificationRead(notification_id)) {
            cout << "[TaskFlow] Marked notification as read: " << notification_id << endl;
            return json{{"success", true}, {"message", "Notification marked as read"}}.dump();
        } else {
            return json{{"success", false}, {"error", "Failed to mark notification as read"}}.dump();
        }
    });
    
    // 33. Mark All Notifications as Read
    server.route("PUT", "/api/notifications/read-all", [&](const string& body, const map<string, string>& headers, const string& path) {
        string token = extract_token(headers);
        auto claims = jwt.validateToken(token);
        
        if (claims.empty()) {
            return json{{"success", false}, {"error", "Authentication required"}}.dump();
        }
        
        if (task_db.markAllNotificationsRead(claims["user_id"])) {
            cout << "[TaskFlow] Marked all notifications as read for user: " << claims["user_id"] << endl;
            return json{{"success", true}, {"message", "All notifications marked as read"}}.dump();
        } else {
            return json{{"success", false}, {"error", "Failed to mark notifications as read"}}.dump();
        }
    });
    
    // ========== TAG MANAGEMENT ==========
    
    // 34. Add Tag to Task
    server.route("POST", "/api/tasks/{id}/tags", [&](const string& body, const map<string, string>& headers, const string& path) {
        string token = extract_token(headers);
        auto claims = jwt.validateToken(token);
        
        if (claims.empty()) {
            return json{{"success", false}, {"error", "Authentication required"}}.dump();
        }
        
        string task_id = get_path_param(headers, "id");
        if (task_id.empty()) {
            return json{{"success", false}, {"error", "Task ID required"}}.dump();
        }
        
        if (!can_edit_task(task_db, task_id, claims["user_id"])) {
            return json{{"success", false}, {"error", "Permission denied to edit task"}}.dump();
        }
        
        try {
            auto data = json::parse(body);
            
            if (!data.contains("tag") || data["tag"].empty()) {
                return json{{"success", false}, {"error", "Tag required"}}.dump();
            }
            
            string tag = data["tag"];
            
            if (task_db.addTaskTag(task_id, tag)) {
                cout << "[TaskFlow] Added tag '" << tag << "' to task " << task_id << endl;
                return json{
                    {"success", true},
                    {"message", "Tag added successfully"},
                    {"tag", tag}
                }.dump();
            } else {
                return json{{"success", false}, {"error", "Failed to add tag"}}.dump();
            }
            
        } catch (const exception& e) {
            return json{{"success", false}, {"error", "Invalid request: " + string(e.what())}}.dump();
        }
    });
    
    // 35. Remove Tag from Task
    server.route("DELETE", "/api/tasks/{id}/tags/{tag}", [&](const string& body, const map<string, string>& headers, const string& path) {
        string token = extract_token(headers);
        auto claims = jwt.validateToken(token);
        
        if (claims.empty()) {
            return json{{"success", false}, {"error", "Authentication required"}}.dump();
        }
        
        string task_id = get_path_param(headers, "id");
        string tag = get_path_param(headers, "tag");
        
        if (task_id.empty() || tag.empty()) {
            return json{{"success", false}, {"error", "Task ID and Tag required"}}.dump();
        }
        
        if (!can_edit_task(task_db, task_id, claims["user_id"])) {
            return json{{"success", false}, {"error", "Permission denied to edit task"}}.dump();
        }
        
        if (task_db.removeTaskTag(task_id, tag)) {
            cout << "[TaskFlow] Removed tag '" << tag << "' from task " << task_id << endl;
            return json{
                {"success", true},
                {"message", "Tag removed successfully"},
                {"tag", tag}
            }.dump();
        } else {
            return json{{"success", false}, {"error", "Failed to remove tag"}}.dump();
        }
    });
    
    // 36. Get Task Tags
    server.route("GET", "/api/tasks/{id}/tags", [&](const string& body, const map<string, string>& headers, const string& path) {
        string token = extract_token(headers);
        auto claims = jwt.validateToken(token);
        
        if (claims.empty()) {
            return json{{"success", false}, {"error", "Authentication required"}}.dump();
        }
        
        string task_id = get_path_param(headers, "id");
        if (task_id.empty()) {
            return json{{"success", false}, {"error", "Task ID required"}}.dump();
        }
        
        if (!can_access_task(task_db, task_id, claims["user_id"])) {
            return json{{"success", false}, {"error", "Access denied to task"}}.dump();
        }
        
        auto tags = task_db.getTaskTags(task_id);
        
        return json{
            {"success", true},
            {"task_id", task_id},
            {"tags", tags},
            {"count", tags.size()}
        }.dump();
    });
    
    // ========== ЗАПУСК СЕРВЕРА ==========
    
    cout << "\n================================================" << endl;
    cout << "🚀 TASK FLOW AUTH MODULE v4.0" << endl;
    cout << "📡 Full Project & Task Management System" << endl;
    cout << "🌐 Server Port: " << port << endl;
    cout << "🔒 Network: ZeroTier Private VPN" << endl;
    cout << "================================================" << endl;
    cout << "📋 API Endpoints Overview:" << endl;
    cout << endl;
    cout << "  AUTHENTICATION (9 endpoints)" << endl;
    cout << "  USER MANAGEMENT (4 endpoints)" << endl;
    cout << "  PROJECT MANAGEMENT (8 endpoints)" << endl;
    cout << "  TASK MANAGEMENT (9 endpoints)" << endl;
    cout << "  COMMENT MANAGEMENT (3 endpoints)" << endl;
    cout << "  NOTIFICATION MANAGEMENT (3 endpoints)" << endl;
    cout << "  TAG MANAGEMENT (3 endpoints)" << endl;
    cout << "  UTILITY (2 endpoints)" << endl;
    cout << endl;
    cout << "  🔗 Total: 41 API endpoints" << endl;
    cout << "================================================\n" << endl;
    
    server.start();
    
    // Keep running
    while (true) {
        this_thread::sleep_for(chrono::seconds(10));
    }
}

// ========== MAIN FUNCTION ==========

int main(int argc, char* argv[]) {
    cout << "========================================" << endl;
    cout << "🚀 Task Flow Auth Module v4.0" << endl;
    cout << "📡 Full Project & Task Management" << endl;
    cout << "🔒 ZeroTier Private Network Integration" << endl;
    cout << "========================================" << endl;
    
    // Parse command line arguments
    bool api_mode = false;
    int api_port = 8081;
    bool useZeroTier = false;
    string ztNetworkId = "";
    string ztApiToken = "";
    string publicUrl = "";
    
    for (int i = 1; i < argc; i++) {
        string arg = argv[i];
        if (arg == "--api" || arg == "-a") {
            api_mode = true;
        } else if ((arg == "--port" || arg == "-p") && i + 1 < argc) {
            api_port = stoi(argv[++i]);
        } else if (arg == "--zerotier" || arg == "-z") {
            useZeroTier = true;
        } else if (arg == "--zerotier-network" && i + 1 < argc) {
            ztNetworkId = argv[++i];
            useZeroTier = true;
        } else if (arg == "--zerotier-token" && i + 1 < argc) {
            ztApiToken = argv[++i];
            useZeroTier = true;
        } else if (arg == "--help" || arg == "-h") {
            cout << "\nUsage:" << endl;
            cout << "  " << argv[0] << "                    - Interactive mode" << endl;
            cout << "  " << argv[0] << " --api             - Start API server" << endl;
            cout << "  " << argv[0] << " --api --port 3000 - Custom port" << endl;
            cout << "  " << argv[0] << " --api --zerotier  - With ZeroTier private network" << endl;
            cout << "  " << argv[0] << " --api --zerotier --zerotier-network NETWORK_ID" << endl;
            cout << "  " << argv[0] << " --help            - Show help" << endl;
            return 0;
        }
    }
    
    try {
        // Initialize core components
        Config config("config.json");
        SimpleDB db(config.getDbFile());
        TaskDB task_db("taskflow_db.json");
        
        // ZeroTier integration
        if (useZeroTier) {
            cout << "\n========================================" << endl;
            cout << "🔧 ZEROTIER PRIVATE NETWORK SETUP" << endl;
            cout << "========================================" << endl;
            
            ZeroTierManager zt;
            
            // Try to initialize ZeroTier
            if (!zt.initialize()) {
                cout << "⚠️  ZeroTier not installed or not running" << endl;
                cout << "📋 Please install ZeroTier One from: https://www.zerotier.com/download/" << endl;
                cout << "📋 Or run download_zerotier.ps1 to install automatically" << endl;
                cout << "📋 Continuing in local-only mode..." << endl;
            } else {
                cout << "✅ ZeroTier initialized successfully" << endl;
                cout << "🔧 Node ID: " << zt.getNodeID() << endl;
                
                // Read network ID from file if not provided via command line
                if (ztNetworkId.empty()) {
                    ifstream network_file("zerotier_network_id.txt");
                    if (network_file) {
                        getline(network_file, ztNetworkId);
                        network_file.close();
                        cout << "📋 Read network ID from file: " << ztNetworkId << endl;
                    }
                }
                
                if (ztNetworkId.empty()) {
                    cout << "⚠️  No ZeroTier network ID provided" << endl;
                    cout << "📋 To create a network:" << endl;
                    cout << "   1. Go to https://my.zerotier.com" << endl;
                    cout << "   2. Create a new network" << endl;
                    cout << "   3. Save the 16-character Network ID to 'zerotier_network_id.txt'" << endl;
                    cout << "   4. Run with: --zerotier-network NETWORK_ID" << endl;
                    cout << "📋 Using local-only mode for now..." << endl;
                } else {
                    cout << "🔗 Joining ZeroTier network: " << ztNetworkId << endl;
                    cout << "⏳ This may take up to 30 seconds..." << endl;
                    
                    if (zt.joinNetwork(ztNetworkId)) {
                        vector<string> ips = zt.getLocalIPs();
                        if (!ips.empty()) {
                            publicUrl = "http://" + ips[0] + ":" + to_string(api_port);
                            
                            cout << "\n========================================" << endl;
                            cout << "🌐 ZEROTIER PRIVATE NETWORK READY" << endl;
                            cout << "========================================" << endl;
                            cout << "📡 Your server IP: " << ips[0] << endl;
                            cout << "🔗 Internal URL: " << publicUrl << endl;
                            cout << "🔧 Network ID: " << ztNetworkId << endl;
                            cout << "📋 Share this with your team members:" << endl;
                            cout << endl;
                            cout << "FOR TEAM MEMBERS TO CONNECT:" << endl;
                            cout << "1. Install ZeroTier One from https://www.zerotier.com/download/" << endl;
                            cout << "2. Join the network: zerotier-cli join " << ztNetworkId << endl;
                            cout << "3. Tell me your Node ID (from: zerotier-cli info)" << endl;
                            cout << "4. I'll authorize you in ZeroTier control panel" << endl;
                            cout << "5. Once authorized, access: " << publicUrl << endl;
                            cout << "========================================\n" << endl;
                        }
                    } else {
                        cout << "❌ Failed to join ZeroTier network" << endl;
                        cout << "📋 Check network ID and authorization" << endl;
                        cout << "📋 Continuing in local-only mode..." << endl;
                    }
                }
            }
            cout << "========================================\n" << endl;
        }
        
        // Set GitHub redirect URI
        string githubRedirectUri;
        if (!publicUrl.empty() && publicUrl.find("http://") == 0) {
            githubRedirectUri = publicUrl + "/api/auth/callback";
        } else {
            githubRedirectUri = config.getGithubRedirectUri();
            if (githubRedirectUri == "auto" || githubRedirectUri.find("/callback") == string::npos) {
                githubRedirectUri = "http://localhost:" + to_string(api_port) + "/api/auth/callback";
            }
        }
        
        cout << "🔗 GitHub OAuth redirect URI: " << githubRedirectUri << endl;
        
        GitHubOAuth github(config.getGithubClientId(),
                          config.getGithubClientSecret(),
                          githubRedirectUri);
        
        JWT jwt(config.getJwtSecret(), config.getJwtExpiryHours());
        
        // Initialize databases
        db.initializeDB();
        task_db.initializeDB();
        
        cout << "✅ System initialized successfully" << endl;
        cout << "📊 Stats:" << endl;
        cout << "  • API Port: " << api_port << endl;
        cout << "  • User DB: " << config.getDbFile() << " (" << db.getAllUsers().size() << " users)" << endl;
        cout << "  • Task DB: taskflow_db.json" << endl;
        cout << "  • ZeroTier: " << (useZeroTier ? "ENABLED" : "DISABLED") << endl;
        if (!publicUrl.empty()) {
            cout << "  • Private URL: " << publicUrl << endl;
        }
        cout << "========================================\n" << endl;
        
        if (api_mode) {
            // Run in API server mode
            runFullTaskFlowServer(api_port, jwt, db, task_db, github);
        } else {
            // Interactive mode (упрощенный для Task Flow)
            cout << "\n=== Task Flow Interactive Mode ===" << endl;
            cout << "Type 'api-start' to launch full API server" << endl;
            cout << "Type 'zerotier-setup' for ZeroTier configuration" << endl;
            cout << "Type 'exit' to quit" << endl;
            cout << "=================================\n" << endl;
            
            AuthSessionManager sessionManager;
            string command;
            
            while (true) {
                cout << "taskflow> ";
                getline(cin, command);
                
                if (command == "api-start") {
                    cout << "🚀 Starting Task Flow API server on port " << api_port << "..." << endl;
                    runFullTaskFlowServer(api_port, jwt, db, task_db, github);
                    break;
                }
                else if (command == "zerotier-setup") {
                    cout << "\n=== ZeroTier Setup ===" << endl;
                    cout << "1. Install ZeroTier from https://www.zerotier.com/download/" << endl;
                    cout << "2. Create network at https://my.zerotier.com" << endl;
                    cout << "3. Save Network ID to 'zerotier_network_id.txt'" << endl;
                    cout << "4. Restart with: auth_module.exe --api --zerotier" << endl;
                    cout << "=====================\n" << endl;
                }
                else if (command == "exit" || command == "quit") {
                    cout << "👋 Goodbye!" << endl;
                    break;
                }
                else if (command == "stats") {
                    cout << "\n=== Database Statistics ===" << endl;
                    cout << "Users: " << db.getAllUsers().size() << endl;
                    cout << "Projects: " << task_db.getAllProjects().size() << endl;
                    cout << "Tasks: " << task_db.getProjectTasks("").size() << endl;
                    cout << "=========================\n" << endl;
                }
                else if (!command.empty()) {
                    cout << "Available commands:" << endl;
                    cout << "  api-start       - Launch API server" << endl;
                    cout << "  zerotier-setup  - Configure ZeroTier" << endl;
                    cout << "  stats           - Show database statistics" << endl;
                    cout << "  exit            - Quit program" << endl;
                }
            }
        }
        
    } catch (const exception& e) {
        cerr << "❌ Critical Error: " << e.what() << endl;
        return 1;
    }
    
    return 0;
}