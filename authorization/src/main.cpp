#include "../include/Config.h"
#include "../include/GitHubOAuth.h"
#include "../include/JWT.h"
#include "../include/SimpleDB.h"
#include "../include/precompiled.h"
#include <iostream>
#include <sstream>
#include <map>
#include <ctime>

using namespace std;

void printHelp() {
    cout << "\n=== Auth Module Commands ===" << endl;
    cout << "1. help          - Show this help" << endl;
    cout << "2. test          - Test configuration" << endl;
    cout << "3. github-auth   - Get GitHub auth URL" << endl;
    cout << "4. callback CODE - Process GitHub callback (replace CODE with actual code)" << endl;
    cout << "5. validate TOKEN- Validate JWT token" << endl;
    cout << "6. users         - List all users" << endl;
    cout << "7. exit          - Exit program" << endl;
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
                                    github_user.name);
    
    if (user.id.empty()) {
        cout << "Error: Failed to save user" << endl;
        return;
    }
    
    map<string, string> jwt_payload = {
        {"user_id", user.id},
        {"username", user.username},
        {"email", user.email},
        {"role", user.role}
    };
    
    string jwt_token = jwt.generateToken(jwt_payload);
    string refresh_token = jwt.generateRefreshToken();
    
    db.saveRefreshToken(user.id, refresh_token);
    
    cout << "\n=== Authentication Successful ===" << endl;
    cout << "User ID: " << user.id << endl;
    cout << "JWT Token: " << jwt_token << endl;
    cout << "Refresh Token: " << refresh_token << endl;
    cout << "===============================\n" << endl;
}

void validateToken(const string& token, JWT& jwt) {
    cout << "\nValidating token..." << endl;
    
    auto claims = jwt.validateToken(token);
    if (claims.empty()) {
        cout << "Token is INVALID or EXPIRED" << endl;
    } else {
        cout << "Token is VALID" << endl;
        cout << "Claims:" << endl;
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
        cout << "---" << endl;
    }
    cout << "=====================\n" << endl;
}

int main() {
    cout << "========================================" << endl;
    cout << "🔐 Student Auth Module v1.0" << endl;
    cout << "========================================" << endl;
    
    try {
        // Initialize components
        Config config("config.json");
        SimpleDB db(config.getDbFile());
        GitHubOAuth github(config.getGithubClientId(),
                          config.getGithubClientSecret(),
                          config.getGithubRedirectUri());
        JWT jwt(config.getJwtSecret(), config.getJwtExpiryHours());
        
        db.initializeDB();
        
        cout << "✅ System initialized successfully" << endl;
        cout << "Port: " << config.getServerPort() << endl;
        cout << "Database: " << config.getDbFile() << endl;
        cout << "========================================\n" << endl;
        
        printHelp();
        
        // Simple command loop
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
            else if (command == "exit" || command == "quit") {
                cout << "Goodbye!" << endl;
                break;
            }
            else if (!command.empty()) {
                cout << "Unknown command. Type 'help' for commands." << endl;
            }
        }
        
    } catch (const exception& e) {
        cerr << "❌ Error: " << e.what() << endl;
        return 1;
    }
    
    return 0;
}