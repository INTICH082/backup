#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <windows.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#endif

#include "../include/Config.h"
#include "../include/GitHubOAuth.h"
#include "../include/JWT.h"
#include "../include/SimpleDB.h"
#include "../include/precompiled.h"

#include <iostream>
#include <sstream>
#include <map>
#include <ctime>
#include <thread>
#include <vector>
#include <algorithm>

using namespace std;
using json = nlohmann::json;

// ========== ПРОСТОЙ HTTP СЕРВЕР ==========

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
            
            // Remove query string from path
            size_t qmark = path.find('?');
            if (qmark != string::npos) path = path.substr(0, qmark);
            
            // Parse headers
            string headers_text;
            string line;
            while (getline(req_stream, line) && line != "\r" && !line.empty()) {
                headers_text += line + "\n";
            }
            
            auto headers = parse_headers(headers_text);
            
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
        setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt));
        
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

void runFullAPIServer(int port, JWT& jwt, SimpleDB& db) {
    SimpleHTTPServer server(port);
    
    // ========== PUBLIC ENDPOINTS ==========
    
    // Health check
    server.route("GET", "/health", [](const string& body, const map<string, string>& headers) {
        return json{{"status", "ok"}, {"service", "auth"}, {"timestamp", time(nullptr)}}.dump();
    });
    
    // Login
    server.route("POST", "/api/auth/login", [&](const string& body, const map<string, string>& headers) {
        try {
            auto data = json::parse(body);
            string username = data["username"];
            string password = data["password"];
            
            auto users = db.getAllUsers();
            User found_user;
            
            for (const auto& user : users) {
                if (user.username == username && user.password_hash == password) {
                    found_user = user;
                    break;
                }
            }
            
            if (found_user.id.empty()) {
                return json{{"success", false}, {"error", "Invalid credentials"}}.dump();
            }
            
            map<string, string> payload = {
                {"user_id", found_user.id},
                {"username", found_user.username},
                {"email", found_user.email},
                {"fullname", found_user.full_name},
                {"role", found_user.role},
                {"course", found_user.course}
            };
            
            string token = jwt.generateToken(payload);
            string refresh_token = jwt.generateRefreshToken();
            db.saveRefreshToken(found_user.id, refresh_token);
            
            return json{
                {"success", true},
                {"token", token},
                {"refresh_token", refresh_token},
                {"user", {
                    {"id", found_user.id},
                    {"username", found_user.username},
                    {"email", found_user.email},
                    {"full_name", found_user.full_name},
                    {"role", found_user.role},
                    {"course", found_user.course}
                }}
            }.dump();
        } catch (...) {
            return json{{"error", "Invalid request"}}.dump();
        }
    });
    
    // Validate token
    server.route("POST", "/api/auth/validate", [&](const string& body, const map<string, string>& headers) {
        string token = extract_token(headers);
        if (token.empty()) {
            return json{{"valid", false}, {"error", "No token"}}.dump();
        }
        
        auto claims = jwt.validateToken(token);
        if (claims.empty()) {
            return json{{"valid", false}, {"error", "Invalid token"}}.dump();
        }
        
        return json{{"valid", true}, {"user", claims}}.dump();
    });
    
    // Register new user
    server.route("POST", "/api/users/register", [&](const string& body, const map<string, string>& headers) {
        try {
            auto data = json::parse(body);
            string username = data["username"];
            string email = data["email"];
            string full_name = data["full_name"];
            string password = data["password"];
            string course = data.value("course", "1");
            string role = data.value("role", "student");
            
            // Check if exists
            auto users = db.getAllUsers();
            for (const auto& user : users) {
                if (user.username == username || user.email == email) {
                    return json{{"success", false}, {"error", "User exists"}}.dump();
                }
            }
            
            User new_user = db.createUserWithPassword(username, email, full_name, password, course, role);
            
            return json{
                {"success", true},
                {"message", "User created"},
                {"user_id", new_user.id}
            }.dump();
        } catch (...) {
            return json{{"error", "Invalid data"}}.dump();
        }
    });
    
    // ========== PROTECTED ENDPOINTS ==========
    
    // Get current user
    server.route("GET", "/api/users/me", [&](const string& body, const map<string, string>& headers) {
        string token = extract_token(headers);
        if (token.empty()) {
            return json{{"error", "Unauthorized"}}.dump();
        }
        
        auto claims = jwt.validateToken(token);
        if (claims.empty()) {
            return json{{"error", "Invalid token"}}.dump();
        }
        
        User user = db.getUserById(claims["user_id"]);
        if (user.id.empty()) {
            return json{{"error", "User not found"}}.dump();
        }
        
        return json{
            {"id", user.id},
            {"username", user.username},
            {"email", user.email},
            {"full_name", user.full_name},
            {"role", user.role},
            {"course", user.course},
            {"github_id", user.github_id}
        }.dump();
    });
    
    // Get all users (admin only)
    server.route("GET", "/api/users", [&](const string& body, const map<string, string>& headers) {
        string token = extract_token(headers);
        if (token.empty()) {
            return json{{"error", "Unauthorized"}}.dump();
        }
        
        auto claims = jwt.validateToken(token);
        if (claims.empty() || claims["role"] != "admin") {
            return json{{"error", "Admin required"}}.dump();
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
                {"course", user.course}
            });
        }
        
        return users_json.dump();
    });
    
    // Refresh token
    server.route("POST", "/api/auth/refresh", [&](const string& body, const map<string, string>& headers) {
        try {
            auto data = json::parse(body);
            string refresh_token = data["refresh_token"];
            string user_id = data["user_id"];
            
            if (!db.validateRefreshToken(user_id, refresh_token)) {
                return json{{"error", "Invalid refresh token"}}.dump();
            }
            
            User user = db.getUserById(user_id);
            if (user.id.empty()) {
                return json{{"error", "User not found"}}.dump();
            }
            
            map<string, string> payload = {
                {"user_id", user.id},
                {"username", user.username},
                {"email", user.email},
                {"fullname", user.full_name},
                {"role", user.role},
                {"course", user.course}
            };
            
            string new_token = jwt.generateToken(payload);
            string new_refresh = jwt.generateRefreshToken();
            
            db.revokeRefreshToken(user_id);
            db.saveRefreshToken(user_id, new_refresh);
            
            return json{
                {"success", true},
                {"token", new_token},
                {"refresh_token", new_refresh}
            }.dump();
        } catch (...) {
            return json{{"error", "Invalid request"}}.dump();
        }
    });
    
    // Start server
    cout << "\n========================================" << endl;
    cout << "🌐 Auth API Server started on port " << port << endl;
    cout << "========================================" << endl;
    cout << "📋 Available endpoints for other modules:" << endl;
    cout << "  POST /api/auth/login      - Login (get JWT)" << endl;
    cout << "  POST /api/auth/validate   - Validate JWT" << endl;
    cout << "  POST /api/auth/refresh    - Refresh token" << endl;
    cout << "  GET  /api/users/me        - Get current user" << endl;
    cout << "  POST /api/users/register  - Register new user" << endl;
    cout << "  GET  /api/users           - Get all users (admin)" << endl;
    cout << "  GET  /health              - Health check" << endl;
    cout << "========================================\n" << endl;
    
    server.start();
    
    // Keep running
    while (true) {
        this_thread::sleep_for(chrono::seconds(10));
    }
}

// ========== [Остальной код интерактивного режима] ==========
// Вставьте сюда ваши функции: printHelp(), testConfiguration() и т.д.

int main(int argc, char* argv[]) {
    cout << "🔐 Student Auth Module v2.0" << endl;
    cout << "📡 Full HTTP API for other modules" << endl;
    
    bool api_mode = false;
    int api_port = 8081;
    
    for (int i = 1; i < argc; i++) {
        string arg = argv[i];
        if (arg == "--api" || arg == "-a") api_mode = true;
        else if ((arg == "--port" || arg == "-p") && i + 1 < argc) api_port = stoi(argv[++i]);
        else if (arg == "--help") {
            cout << "Usage: auth_module.exe --api [--port 8081]" << endl;
            return 0;
        }
    }
    
    try {
        Config config("config.json");
        SimpleDB db(config.getDbFile());
        GitHubOAuth github(config.getGithubClientId(),
                          config.getGithubClientSecret(),
                          config.getGithubRedirectUri());
        JWT jwt(config.getJwtSecret(), config.getJwtExpiryHours());
        
        db.initializeDB();
        
        cout << "✅ System ready on port " << api_port << endl;
        
        if (api_mode) {
            runFullAPIServer(api_port, jwt, db);
        } else {
            // [Ваш интерактивный режим]
            cout << "Interactive mode not implemented for API server" << endl;
        }
        
    } catch (const exception& e) {
        cerr << "❌ Error: " << e.what() << endl;
        return 1;
    }
    
    return 0;
}