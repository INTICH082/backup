#include "../include/Config.h"
#include "../include/AuthServer.h"
#include "../include/GitHubOAuth.h"
#include "../include/JWT.h"
#include "../include/SimpleDB.h"
#include <iostream>
#include <sstream>
#include <map>
#include <ctime>

using namespace std;

int main() {
    try {
        // ВАША КОНФИГУРАЦИЯ - файл config.json
        Config config("config.json");
        
        cout << "=========================================" << endl;
        cout << "Starting Auth Server v1.0" << endl;
        cout << "=========================================" << endl;
        cout << "GitHub Client ID: " << config.getGithubClientId() << endl;
        cout << "Server: " << config.getServerHost() << ":" << config.getServerPort() << endl;
        cout << "=========================================" << endl;
        
        // Инициализация базы данных (файловое хранилище)
        SimpleDB db(config.getDbFile());
        db.initializeDB();
        
        // ВАШ GitHub OAuth
        GitHubOAuth github_oauth(config.getGithubClientId(),
                               config.getGithubClientSecret(),
                               config.getGithubRedirectUri());
        
        // ВАШ JWT секрет
        JWT jwt(config.getJwtSecret(), config.getJwtExpiryHours());
        
        AuthServer server(config.getServerPort(), config.getServerHost());
        
        // Главная страница
        server.addRoute("/", [](const map<string, string>& params) {
            string html = R"(
            <html>
            <head>
                <title>Auth Server - Student Testing System</title>
                <style>
                    body { font-family: Arial, sans-serif; margin: 40px; }
                    h1 { color: #333; }
                    .endpoint { background: #f5f5f5; padding: 10px; margin: 10px 0; }
                    code { background: #eee; padding: 2px 4px; }
                </style>
            </head>
            <body>
                <h1>🔐 Auth Server - Student Testing System</h1>
                <p>This is the authentication server for the student project.</p>
                
                <h2>API Endpoints:</h2>
                <div class="endpoint">
                    <strong>GET</strong> <code><a href="/health">/health</a></code><br>
                    Health check endpoint
                </div>
                
                <div class="endpoint">
                    <strong>GET</strong> <code><a href="/auth/github">/auth/github</a></code><br>
                    Start GitHub OAuth authentication
                </div>
                
                <div class="endpoint">
                    <strong>GET</strong> <code>/auth/validate?token=JWT_TOKEN</code><br>
                    Validate JWT token
                </div>
                
                <div class="endpoint">
                    <strong>GET</strong> <code>/auth/user?user_id=USER_ID</code><br>
                    Get user information
                </div>
                
                <div class="endpoint">
                    <strong>POST</strong> <code>/auth/refresh?refresh_token=TOKEN&user_id=ID</code><br>
                    Refresh access token
                </div>
                
                <h2>Test Links:</h2>
                <ul>
                    <li><a href="/test/users">View all users (for testing)</a></li>
                    <li><a href="/test/db">View database state</a></li>
                </ul>
                
                <hr>
                <p><small>Student Project - Authentication Module</small></p>
            </body>
            </html>
            )";
            
            stringstream response;
            response << "HTTP/1.1 200 OK\r\n";
            response << "Content-Type: text/html\r\n";
            response << "Content-Length: " << html.length() << "\r\n";
            response << "\r\n";
            response << html;
            
            return response.str();
        });
        
        // Health check
        server.addRoute("/health", [](const map<string, string>& params) {
            json response;
            response["status"] = "ok";
            response["service"] = "auth_server";
            response["timestamp"] = time(nullptr);
            response["version"] = "1.0.0";
            
            string json_str = response.dump(2);
            
            stringstream res;
            res << "HTTP/1.1 200 OK\r\n";
            res << "Content-Type: application/json\r\n";
            res << "Access-Control-Allow-Origin: *\r\n";
            res << "Content-Length: " << json_str.length() << "\r\n";
            res << "\r\n";
            res << json_str;
            
            return res.str();
        });
        
        // GitHub OAuth flow
        server.addRoute("/auth/github", [&github_oauth](const map<string, string>& params) {
            cout << "GitHub OAuth requested" << endl;
            string state = to_string(time(nullptr));
            string auth_url = github_oauth.getAuthorizationUrl(state);
            cout << "Redirecting to: " << auth_url << endl;
            return server.sendRedirect(auth_url);
        });
        
        server.addRoute("/auth/github/callback", [&](const map<string, string>& params) {
            cout << "GitHub callback received" << endl;
            
            if (params.find("code") == params.end()) {
                cout << "Error: No code parameter" << endl;
                return server.sendErrorResponse("Missing authorization code", 400);
            }
            
            string code = params.at("code");
            cout << "GitHub code received: " << code.substr(0, 15) << "..." << endl;
            
            // ВАШ запрос к GitHub для получения токена
            string access_token = github_oauth.getAccessToken(code);
            
            if (access_token.empty()) {
                cout << "Error: Failed to get access token" << endl;
                return server.sendErrorResponse("Failed to get access token from GitHub. Check your Client ID and Secret.", 401);
            }
            
            cout << "GitHub access token obtained successfully" << endl;
            
            // ВАШ запрос к GitHub API для получения информации о пользователе
            GitHubUser github_user = github_oauth.getUserInfo(access_token);
            
            if (github_user.id.empty()) {
                cout << "Error: Failed to get user info" << endl;
                return server.sendErrorResponse("Failed to get user info from GitHub", 401);
            }
            
            cout << "GitHub user authenticated: " << github_user.login 
                 << " (" << github_user.name << ")" << endl;
            
            // Сохраняем пользователя в БД
            User user = db.createOrUpdateUser(github_user.id,
                                            github_user.login,
                                            github_user.email,
                                            github_user.name);
            
            if (user.id.empty()) {
                cout << "Error: Failed to save user to database" << endl;
                return server.sendErrorResponse("Failed to save user to database", 500);
            }
            
            cout << "User saved to database with ID: " << user.id << endl;
            
            // Генерируем JWT токен с ВАШИМ секретом
            map<string, string> jwt_payload = {
                {"user_id", user.id},
                {"username", user.username},
                {"email", user.email},
                {"role", user.role}
            };
            
            string jwt_token = jwt.generateToken(jwt_payload);
            string refresh_token = jwt.generateRefreshToken();
            
            // Сохраняем токены
            db.saveRefreshToken(user.id, refresh_token);
            db.createSession(user.id, jwt_token);
            
            // Формируем ответ
            json response;
            response["access_token"] = jwt_token;
            response["refresh_token"] = refresh_token;
            response["token_type"] = "Bearer";
            response["expires_in"] = config.getJwtExpiryHours() * 3600;
            
            json user_json;
            user_json["id"] = user.id;
            user_json["github_id"] = user.github_id;
            user_json["username"] = user.username;
            user_json["email"] = user.email;
            user_json["full_name"] = user.full_name;
            user_json["role"] = user.role;
            
            response["user"] = user_json;
            
            string json_str = response.dump(2);
            
            cout << "Authentication successful for user: " << user.username << endl;
            
            stringstream res;
            res << "HTTP/1.1 200 OK\r\n";
            res << "Content-Type: application/json\r\n";
            res << "Access-Control-Allow-Origin: *\r\n";
            res << "Content-Length: " << json_str.length() << "\r\n";
            res << "\r\n";
            res << json_str;
            
            return res.str();
        });
        
        // Валидация токена
        server.addRoute("/auth/validate", [&](const map<string, string>& params) {
            if (params.find("token") == params.end()) {
                return server.sendErrorResponse("Missing token parameter", 400);
            }
            
            string token = params.at("token");
            cout << "Token validation requested" << endl;
            
            auto claims = jwt.validateToken(token);
            
            if (claims.empty()) {
                cout << "Token invalid or expired" << endl;
                return server.sendErrorResponse("Invalid or expired JWT token", 401);
            }
            
            if (!db.validateSession(token)) {
                cout << "Session invalid" << endl;
                return server.sendErrorResponse("Session expired or invalid", 401);
            }
            
            json response;
            response["valid"] = true;
            response["user"] = claims;
            
            string json_str = response.dump(2);
            
            stringstream res;
            res << "HTTP/1.1 200 OK\r\n";
            res << "Content-Type: application/json\r\n";
            res << "Access-Control-Allow-Origin: *\r\n";
            res << "Content-Length: " << json_str.length() << "\r\n";
            res << "\r\n";
            res << json_str;
            
            return res.str();
        });
        
        // Обновление токена
        server.addRoute("/auth/refresh", [&](const map<string, string>& params) {
            if (params.find("refresh_token") == params.end() || 
                params.find("user_id") == params.end()) {
                return server.sendErrorResponse("Missing refresh_token or user_id", 400);
            }
            
            string refresh_token = params.at("refresh_token");
            string user_id = params.at("user_id");
            
            cout << "Token refresh requested for user: " << user_id << endl;
            
            if (!db.validateRefreshToken(user_id, refresh_token)) {
                return server.sendErrorResponse("Invalid or expired refresh token", 401);
            }
            
            User user = db.getUserById(user_id);
            if (user.id.empty()) {
                return server.sendErrorResponse("User not found", 404);
            }
            
            map<string, string> jwt_payload = {
                {"user_id", user.id},
                {"username", user.username},
                {"email", user.email},
                {"role", user.role}
            };
            
            string new_jwt_token = jwt.generateToken(jwt_payload);
            string new_refresh_token = jwt.generateRefreshToken();
            
            db.revokeRefreshToken(user.id);
            db.saveRefreshToken(user.id, new_refresh_token);
            db.createSession(user.id, new_jwt_token);
            
            json response;
            response["access_token"] = new_jwt_token;
            response["refresh_token"] = new_refresh_token;
            response["token_type"] = "Bearer";
            response["expires_in"] = config.getJwtExpiryHours() * 3600;
            
            string json_str = response.dump(2);
            
            cout << "Token refreshed for user: " << user.username << endl;
            
            stringstream res;
            res << "HTTP/1.1 200 OK\r\n";
            res << "Content-Type: application/json\r\n";
            res << "Access-Control-Allow-Origin: *\r\n";
            res << "Content-Length: " << json_str.length() << "\r\n";
            res << "\r\n";
            res << json_str;
            
            return res.str();
        });
        
        // Выход из системы
        server.addRoute("/auth/logout", [&](const map<string, string>& params) {
            if (params.find("token") == params.end()) {
                return server.sendErrorResponse("Missing token parameter", 400);
            }
            
            string token = params.at("token");
            db.deleteSession(token);
            
            if (params.find("user_id") != params.end()) {
                string user_id = params.at("user_id");
                db.revokeRefreshToken(user_id);
                cout << "User logged out: " << user_id << endl;
            }
            
            json response;
            response["message"] = "Logged out successfully";
            
            string json_str = response.dump(2);
            
            stringstream res;
            res << "HTTP/1.1 200 OK\r\n";
            res << "Content-Type: application/json\r\n";
            res << "Access-Control-Allow-Origin: *\r\n";
            res << "Content-Length: " << json_str.length() << "\r\n";
            res << "\r\n";
            res << json_str;
            
            return res.str();
        });
        
        // Получение информации о пользователе
        server.addRoute("/auth/user", [&](const map<string, string>& params) {
            if (params.find("user_id") == params.end()) {
                return server.sendErrorResponse("Missing user_id parameter", 400);
            }
            
            string user_id = params.at("user_id");
            User user = db.getUserById(user_id);
            
            if (user.id.empty()) {
                return server.sendErrorResponse("User not found", 404);
            }
            
            json response;
            response["id"] = user.id;
            response["github_id"] = user.github_id;
            response["username"] = user.username;
            response["email"] = user.email;
            response["full_name"] = user.full_name;
            response["role"] = user.role;
            
            string json_str = response.dump(2);
            
            stringstream res;
            res << "HTTP/1.1 200 OK\r\n";
            res << "Content-Type: application/json\r\n";
            res << "Access-Control-Allow-Origin: *\r\n";
            res << "Content-Length: " << json_str.length() << "\r\n";
            res << "\r\n";
            res << json_str;
            
            return res.str();
        });
        
        // Тестовые эндпоинты (для разработки)
        server.addRoute("/test/users", [&](const map<string, string>& params) {
            vector<User> users = db.getAllUsers();
            
            json response = json::array();
            for (const auto& user : users) {
                json user_json;
                user_json["id"] = user.id;
                user_json["github_id"] = user.github_id;
                user_json["username"] = user.username;
                user_json["email"] = user.email;
                user_json["full_name"] = user.full_name;
                user_json["role"] = user.role;
                response.push_back(user_json);
            }
            
            string json_str = response.dump(2);
            
            stringstream res;
            res << "HTTP/1.1 200 OK\r\n";
            res << "Content-Type: application/json\r\n";
            res << "Content-Length: " << json_str.length() << "\r\n";
            res << "\r\n";
            res << json_str;
            
            return res.str();
        });
        
        server.addRoute("/test/db", [&](const map<string, string>& params) {
            ifstream file(config.getDbFile());
            if (!file.is_open()) {
                return server.sendErrorResponse("Database file not found", 404);
            }
            
            json db_data;
            file >> db_data;
            
            string json_str = db_data.dump(2);
            
            stringstream res;
            res << "HTTP/1.1 200 OK\r\n";
            res << "Content-Type: application/json\r\n";
            res << "Content-Length: " << json_str.length() << "\r\n";
            res << "\r\n";
            res << json_str;
            
            return res.str();
        });
        
        cout << "\n✅ Auth Server ready!" << endl;
        cout << "📡 Listening on: http://" << config.getServerHost() << ":" << config.getServerPort() << endl;
        cout << "🔗 GitHub OAuth: http://" << config.getServerHost() << ":" << config.getServerPort() << "/auth/github" << endl;
        cout << "💾 Database file: " << config.getDbFile() << endl;
        cout << "=========================================" << endl;
        
        server.start();
        
        cout << "\nPress Enter to stop the server..." << endl;
        cin.get();
        
        server.stop();
        cout << "Server stopped. Goodbye!" << endl;
        
    } catch (const exception& e) {
        cerr << "❌ Fatal error: " << e.what() << endl;
        return 1;
    }
    
    return 0;
}