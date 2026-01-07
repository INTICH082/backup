#include "../include/SimpleDB.h"
#include <ctime>
#include <random>
#include <algorithm>

SimpleDB::SimpleDB(const string& db_file) : db_file(db_file) {
    loadDB();
}

void SimpleDB::loadDB() {
    ifstream file(db_file);
    if (file.is_open()) {
        try {
            file >> data;
        } catch (...) {
            data = json::object();
            data["users"] = json::array();
            data["tokens"] = json::array();
            data["sessions"] = json::array();
        }
    } else {
        data = json::object();
        data["users"] = json::array();
        data["tokens"] = json::array();
        data["sessions"] = json::array();
        saveDB();
    }
}

void SimpleDB::saveDB() {
    ofstream file(db_file);
    if (file.is_open()) {
        file << data.dump(4);
    }
}

string SimpleDB::generateId() {
    static const char charset[] = "0123456789abcdef";
    string id;
    
    random_device rd;
    mt19937 gen(rd());
    uniform_int_distribution<> dis(0, sizeof(charset) - 2);
    
    for (int i = 0; i < 24; ++i) {
        id += charset[dis(gen)];
    }
    
    return id;
}

bool SimpleDB::testConnection() {
    return true; // Всегда доступно для файлового хранилища
}

void SimpleDB::initializeDB() {
    // Убедимся что структуры данных существуют
    if (!data.contains("users")) {
        data["users"] = json::array();
    }
    if (!data.contains("tokens")) {
        data["tokens"] = json::array();
    }
    if (!data.contains("sessions")) {
        data["sessions"] = json::array();
    }
    saveDB();
    cout << "Database initialized successfully" << endl;
}

User SimpleDB::createOrUpdateUser(const string& github_id,
                                const string& username,
                                const string& email,
                                const string& full_name) {
    
    User existing_user = getUserByGithubId(github_id);
    
    if (!existing_user.id.empty()) {
        // Update existing user
        for (auto& user : data["users"]) {
            if (user["github_id"] == github_id) {
                user["username"] = username;
                user["email"] = email;
                user["full_name"] = full_name;
                saveDB();
                
                existing_user.username = username;
                existing_user.email = email;
                existing_user.full_name = full_name;
                return existing_user;
            }
        }
    } else {
        // Create new user
        User new_user;
        new_user.id = generateId();
        new_user.github_id = github_id;
        new_user.username = username;
        new_user.email = email;
        new_user.full_name = full_name;
        new_user.role = "student";
        
        json user_json;
        user_json["id"] = new_user.id;
        user_json["github_id"] = github_id;
        user_json["username"] = username;
        user_json["email"] = email;
        user_json["full_name"] = full_name;
        user_json["role"] = "student";
        
        data["users"].push_back(user_json);
        saveDB();
        
        return new_user;
    }
    
    return User{};
}

User SimpleDB::getUserById(const string& user_id) {
    for (const auto& user : data["users"]) {
        if (user["id"] == user_id) {
            User u;
            u.id = user["id"].get<string>();
            u.github_id = user["github_id"].get<string>();
            u.username = user["username"].get<string>();
            u.email = user["email"].get<string>();
            u.full_name = user["full_name"].get<string>();
            u.role = user["role"].get<string>();
            return u;
        }
    }
    return User{};
}

User SimpleDB::getUserByGithubId(const string& github_id) {
    for (const auto& user : data["users"]) {
        if (user["github_id"] == github_id) {
            User u;
            u.id = user["id"].get<string>();
            u.github_id = user["github_id"].get<string>();
            u.username = user["username"].get<string>();
            u.email = user["email"].get<string>();
            u.full_name = user["full_name"].get<string>();
            u.role = user["role"].get<string>();
            return u;
        }
    }
    return User{};
}

vector<User> SimpleDB::getAllUsers() {
    vector<User> users;
    
    for (const auto& user : data["users"]) {
        User u;
        u.id = user["id"].get<string>();
        u.github_id = user["github_id"].get<string>();
        u.username = user["username"].get<string>();
        u.email = user["email"].get<string>();
        u.full_name = user["full_name"].get<string>();
        u.role = user["role"].get<string>();
        users.push_back(u);
    }
    
    return users;
}

bool SimpleDB::updateUserRole(const string& user_id, const string& role) {
    for (auto& user : data["users"]) {
        if (user["id"] == user_id) {
            user["role"] = role;
            saveDB();
            return true;
        }
    }
    return false;
}

void SimpleDB::saveRefreshToken(const string& user_id, const string& refresh_token) {
    json token;
    token["user_id"] = user_id;
    token["token"] = refresh_token;
    token["created_at"] = time(nullptr);
    
    data["tokens"].push_back(token);
    saveDB();
}

bool SimpleDB::validateRefreshToken(const string& user_id, const string& refresh_token) {
    time_t now = time(nullptr);
    
    for (const auto& token : data["tokens"]) {
        if (token["user_id"] == user_id && token["token"] == refresh_token) {
            time_t created_at = token["created_at"].get<time_t>();
            // Check if token is less than 30 days old
            return (now - created_at) < (30 * 24 * 3600);
        }
    }
    return false;
}

void SimpleDB::revokeRefreshToken(const string& user_id) {
    json new_tokens = json::array();
    
    for (const auto& token : data["tokens"]) {
        if (token["user_id"] != user_id) {
            new_tokens.push_back(token);
        }
    }
    
    data["tokens"] = new_tokens;
    saveDB();
}

void SimpleDB::createSession(const string& user_id, const string& session_token) {
    json session;
    session["user_id"] = user_id;
    session["session_token"] = session_token;
    session["created_at"] = time(nullptr);
    
    data["sessions"].push_back(session);
    saveDB();
}

bool SimpleDB::validateSession(const string& session_token) {
    time_t now = time(nullptr);
    
    for (const auto& session : data["sessions"]) {
        if (session["session_token"] == session_token) {
            time_t created_at = session["created_at"].get<time_t>();
            // Check if session is less than 1 hour old
            return (now - created_at) < 3600;
        }
    }
    return false;
}

void SimpleDB::deleteSession(const string& session_token) {
    json new_sessions = json::array();
    
    for (const auto& session : data["sessions"]) {
        if (session["session_token"] != session_token) {
            new_sessions.push_back(session);
        }
    }
    
    data["sessions"] = new_sessions;
    saveDB();
}