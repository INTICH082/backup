#include "../include/precompiled.h"
#include "../include/Config.h"
#include "../include/GitHubOAuth.h"
#include "../include/JWT.h"
#include "../include/SimpleDB.h"
#include "../include/XTunnelSimple.h"  

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
    
    // GitHub OAuth callback (example)
    server.route("GET", "/auth/github/callback", [&](const string& body, const map<string, string>& headers) {
        // This would handle GitHub OAuth redirect
        return json{{"message", "GitHub OAuth endpoint"}}.dump();
    });
    
    // Discovery endpoint (NEW - для xTunnel)
    server.route("GET", "/api/discovery", [&](const string& body, const map<string, string>& headers) {
        return json{
            {"service", "auth_server"},
            {"version", "2.0"},
            {"timestamp", time(nullptr)},
            {"endpoints", {
                {{"method", "GET"}, {"path", "/health"}, {"description", "Health check"}},
                {{"method", "POST"}, {"path", "/api/auth/login"}, {"description", "User login"}},
                {{"method", "POST"}, {"path", "/api/users/register"}, {"description", "User registration"}},
                {{"method", "GET"}, {"path", "/api/users/me"}, {"description", "Get current user"}},
                {{"method", "GET"}, {"path", "/api/users"}, {"description", "Get all users (admin)"}},
                {{"method", "POST"}, {"path", "/api/auth/validate"}, {"description", "Validate JWT token"}}
            }}
        }.dump();
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
    cout << "  GET  /api/discovery       - Service discovery" << endl;
    cout << "========================================\n" << endl;
    
    server.start();
    
    // Keep running
    while (true) {
        this_thread::sleep_for(chrono::seconds(10));
    }
}

// ========== INTERACTIVE MODE FUNCTIONS ==========

void printHelp() {
    cout << "\n=== Auth Module Commands ===" << endl;
    cout << "1. help          - Show this help" << endl;
    cout << "2. test          - Test configuration" << endl;
    cout << "3. github-auth   - Get GitHub auth URL" << endl;
    cout << "4. callback CODE - Process GitHub callback" << endl;
    cout << "5. validate TOKEN- Validate JWT token" << endl;
    cout << "6. users         - List all users" << endl;
    cout << "7. test-token    - Generate test JWT token" << endl;
    cout << "8. create-user   - Create test user" << endl;
    cout << "9. api-start     - Start API server" << endl;
    cout << "10. exit         - Exit program" << endl;
    cout << "============================\n" << endl;
}

void testConfiguration(Config& config, SimpleDB& db) {
    cout << "\n=== Configuration Test ===" << endl;
    cout << "GitHub Client ID: " << config.getGithubClientId() << endl;
    cout << "JWT Secret: " << (config.getJwtSecret().empty() ? "NOT SET" : "SET") << endl;
    cout << "Database file: " << config.getDbFile() << endl;
    
    if (db.getAllUsers().empty()) {
        cout << "Database: No users yet" << endl;
    } else {
        cout << "Database: " << db.getAllUsers().size() << " users" << endl;
    }
    cout << "==========================\n" << endl;
}

void showGitHubAuthURL(GitHubOAuth& github) {
    cout << "\n=== GitHub Auth URL ===" << endl;
    cout << "Open this URL in browser:" << endl;
    cout << github.getAuthorizationUrl() << endl;
    cout << "========================\n" << endl;
}

void processGitHubCallback(const string& code, 
                          GitHubOAuth& github, 
                          SimpleDB& db, 
                          JWT& jwt) {
    cout << "\nProcessing GitHub callback with code: " << code << endl;
    
    string access_token = github.getAccessToken(code);
    if (access_token.empty()) {
        cout << "Error: Failed to get access token" << endl;
        return;
    }
    
    GitHubUser github_user = github.getUserInfo(access_token);
    if (github_user.id.empty()) {
        cout << "Error: Failed to get user info" << endl;
        return;
    }
    
    cout << "GitHub User: " << github_user.login 
         << " (" << github_user.name << ")" << endl;
    
    User user = db.createOrUpdateUser(github_user.id,
                                    github_user.login,
                                    github_user.email,
                                    github_user.name,
                                    "1",  // default course
                                    ""); // no password for GitHub auth
    
    if (user.id.empty()) {
        cout << "Error: Failed to save user" << endl;
        return;
    }
    
    map<string, string> jwt_payload = {
        {"user_id", user.id},
        {"username", user.username},
        {"email", user.email},
        {"fullname", user.full_name},
        {"role", user.role},
        {"course", user.course}
    };
    
    string jwt_token = jwt.generateToken(jwt_payload);
    string refresh_token = jwt.generateRefreshToken();
    
    db.saveRefreshToken(user.id, refresh_token);
    
    cout << "\n=== Authentication Successful ===" << endl;
    cout << "User ID: " << user.id << endl;
    cout << "Username: " << user.username << endl;
    cout << "Email: " << user.email << endl;
    cout << "Course: " << user.course << endl;
    cout << "Role: " << user.role << endl;
    cout << "JWT Token: " << jwt_token << endl;
    cout << "Refresh Token: " << refresh_token << endl;
    cout << "===============================\n" << endl;
}

void validateToken(const string& token, JWT& jwt) {
    cout << "\nValidating token..." << endl;
    
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
    cout << endl;
}

void listUsers(SimpleDB& db) {
    vector<User> users = db.getAllUsers();
    
    cout << "\n=== Users (" << users.size() << ") ===" << endl;
    for (const auto& user : users) {
        cout << "ID: " << user.id << endl;
        cout << "GitHub: " << user.github_id << " (" << user.username << ")" << endl;
        cout << "Name: " << user.full_name << endl;
        cout << "Email: " << user.email << endl;
        cout << "Role: " << user.role << endl;
        cout << "Course: " << user.course << endl;
        cout << "Password hash: " << (user.password_hash.empty() ? "No" : "Yes") << endl;
        cout << "---" << endl;
    }
    cout << "=====================\n" << endl;
}

void generateTestToken(JWT& jwt) {
    cout << "\n=== Generating Test JWT Token ===" << endl;
    
    map<string, string> test_payload = {
        {"user_id", "test_user_123"},
        {"username", "testuser"},
        {"email", "test@example.com"},
        {"fullname", "Test User"},
        {"role", "student"},
        {"course", "1"}
    };
    
    string test_token = jwt.generateToken(test_payload);
    cout << "Token: " << test_token << endl;
    
    // Show token parts
    size_t dot1 = test_token.find('.');
    size_t dot2 = test_token.find('.', dot1 + 1);
    
    if (dot1 != string::npos && dot2 != string::npos) {
        string header_b64 = test_token.substr(0, dot1);
        string payload_b64 = test_token.substr(dot1 + 1, dot2 - dot1 - 1);
        
        cout << "\nHeader (base64): " << header_b64 << endl;
        cout << "Payload (base64): " << payload_b64 << endl;
        
        cout << "Payload (decoded JSON): {" << endl;
        cout << "  \"user_id\": \"test_user_123\"," << endl;
        cout << "  \"username\": \"testuser\"," << endl;
        cout << "  \"email\": \"test@example.com\"," << endl;
        cout << "  \"fullname\": \"Test User\"," << endl;
        cout << "  \"role\": \"student\"," << endl;
        cout << "  \"course\": \"1\"," << endl;
        cout << "  \"exp\": <timestamp>" << endl;
        cout << "}" << endl;
    }
    
    cout << "\nUse this token for testing other modules:" << endl;
    cout << "Header: Authorization: Bearer " << test_token << endl;
    cout << "========================================\n" << endl;
}

void createTestUser(SimpleDB& db, JWT& jwt) {
    cout << "\n=== Creating Test User ===" << endl;
    
    User test_user = db.createUserWithPassword(
        "demo_user",
        "demo@example.com",
        "Demo User",
        "demo123",  // Plain text password (NOT SECURE - for testing only)
        "1",
        "student"
    );
    
    if (!test_user.id.empty()) {
        cout << "✅ Test user created successfully!" << endl;
        cout << "User ID: " << test_user.id << endl;
        cout << "Username: demo_user" << endl;
        cout << "Password: demo123" << endl;
        cout << "\nUse these credentials for testing login API" << endl;
    } else {
        cout << "❌ Failed to create test user" << endl;
    }
    cout << "===========================\n" << endl;
}

// ========== MAIN FUNCTION ==========

int main(int argc, char* argv[]) {
    cout << "========================================" << endl;
    cout << "🔐 Student Auth Module v2.0" << endl;
    cout << "📡 HTTP API Server for Other Modules" << endl;
    cout << "========================================" << endl;
    
    // Check command line arguments
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
            cout << "  " << argv[0] << " --api             - Start API server on port 8081" << endl;
            cout << "  " << argv[0] << " --api --port 3000 - API server on custom port" << endl;
            cout << "  " << argv[0] << " --api --xtunnel   - API server with xTunnel" << endl;
            cout << "  " << argv[0] << " --api --xtunnel --xtunnel-key YOUR_KEY" << endl;
            cout << "  " << argv[0] << " --help            - Show this help" << endl;
            return 0;
        }
    }
    
    try {
        // Initialize components
        Config config("config.json");
        SimpleDB db(config.getDbFile());
        
        // xTunnel обработка
        if (useXtunnel) {
            cout << "\n========================================" << endl;
            cout << "🔧 XTUNNEL INTEGRATION" << endl;
            cout << "========================================" << endl;
            
            if (!XTunnelSimple::isAvailable()) {
                cout << "❌ xTunnel not found in xtunnel/ or build/ folders" << endl;
                cout << "📥 Download it with: powershell -File download_xtunnel.ps1" << endl;
                cout << "   OR manually from: https://xtunnel.ru" << endl;
                cout << "📋 Continuing in local-only mode..." << endl;
            } else {
                cout << "✅ xTunnel found" << endl;
                
                // Запускаем туннель
                if (XTunnelSimple::startTunnel(api_port, xtunnelKey)) {
                    publicUrl = XTunnelSimple::getTunnelUrl();
                    
                    cout << "\n🌐 PUBLIC URL: " << publicUrl << endl;
                    cout << "\n📋 For your colleagues:" << endl;
                    cout << "API Base URL: " << publicUrl << endl;
                    cout << "Example: " << publicUrl << "/api/auth/login" << endl;
                    cout << "\n💡 Share this URL with your team!" << endl;
                    
                    // Логируем для отладки
                    cout << "\n📝 Log: GitHub callback will use: " 
                         << publicUrl << "/auth/github/callback" << endl;
                } else {
                    cout << "❌ Failed to start xTunnel" << endl;
                }
            }
            cout << "========================================\n" << endl;
        }
        
        // Используем публичный URL для GitHub если есть, иначе из конфига
        string githubRedirectUri;
        if (!publicUrl.empty() && publicUrl.find("https://") == 0) {
            githubRedirectUri = publicUrl + "/auth/github/callback";
        } else {
            githubRedirectUri = config.getGithubRedirectUri();
        }
        
        cout << "🔗 GitHub OAuth redirect URI: " << githubRedirectUri << endl;
        
        GitHubOAuth github(config.getGithubClientId(),
                          config.getGithubClientSecret(),
                          githubRedirectUri);
        
        JWT jwt(config.getJwtSecret(), config.getJwtExpiryHours());
        
        db.initializeDB();
        
        cout << "✅ System initialized successfully" << endl;
        cout << "API Port: " << api_port << endl;
        cout << "Database: " << config.getDbFile() << endl;
        cout << "xTunnel: " << (useXtunnel ? "ENABLED" : "DISABLED") << endl;
        cout << "========================================\n" << endl;
        
        if (api_mode) {
            // Run in API server mode
            runFullAPIServer(api_port, jwt, db);
        } else {
            // Run in interactive mode
            printHelp();
            
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
                else if (command == "github-auth") {
                    showGitHubAuthURL(github);
                }
                else if (command.find("callback ") == 0) {
                    string code = command.substr(9);
                    if (!code.empty()) {
                        processGitHubCallback(code, github, db, jwt);
                    } else {
                        cout << "Error: No code provided" << endl;
                    }
                }
                else if (command.find("validate ") == 0) {
                    string token = command.substr(9);
                    if (!token.empty()) {
                        validateToken(token, jwt);
                    } else {
                        cout << "Error: No token provided" << endl;
                    }
                }
                else if (command == "users") {
                    listUsers(db);
                }
                else if (command == "test-token") {
                    generateTestToken(jwt);
                }
                else if (command == "create-user") {
                    createTestUser(db, jwt);
                }
                else if (command == "api-start") {
                    cout << "Starting API server on port " << api_port << "..." << endl;
                    runFullAPIServer(api_port, jwt, db);
                    break;
                }
                else if (command == "exit" || command == "quit") {
                    cout << "Goodbye!" << endl;
                    break;
                }
                else if (!command.empty()) {
                    cout << "Unknown command. Type 'help' for commands." << endl;
                }
            }
        }
        
    } catch (const exception& e) {
        cerr << "❌ Error: " << e.what() << endl;
        
        // Останавливаем xTunnel при ошибке
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