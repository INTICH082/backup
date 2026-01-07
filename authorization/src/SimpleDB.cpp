#include "../include/SimpleDB.h"
#include "../include/precompiled.h"
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
        }
    } else {
        data = json::object();
        data["users"] = json::array();
        data["tokens"] = json::array();
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

time_t SimpleDB::getCurrentTime() {
    return time(nullptr);
}

void SimpleDB::initializeDB() {
    if (!data.contains("users")) {
        data["users"] = json::array();
    }
    if (!data.contains("tokens")) {
        data["tokens"] = json::array();
    }
    saveDB();
    cout << "Database initialized: " << db_file << endl;
}

User SimpleDB::createOrUpdateUser(const string& github_id,
                                const string& username,
                                const string& email,
                                const string& full_name,
                                const string& course,
                                const string& password_hash) {
    
    User existing_user = getUserByGithubId(github_id);
    
    if (!existing_user.id.empty()) {
        // Update existing user
        for (auto& user : data["users"]) {
            if (user["github_id"] == github_id) {
                user["username"] = username;
                user["email"] = email;
                user["full_name"] = full_name;
                user["course"] = course;
                if (!password_hash.empty()) {
                    user["password_hash"] = password_hash;
                }
                saveDB();
                
                existing_user.username = username;
                existing_user.email = email;
                existing_user.full_name = full_name;
                existing_user.course = course;
                if (!password_hash.empty()) {
                    existing_user.password_hash = password_hash;
                }
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
        new_user.course = course;
        new_user.password_hash = password_hash;
        
        json user_json;
        user_json["id"] = new_user.id;
        user_json["github_id"] = github_id;
        user_json["username"] = username;
        user_json["email"] = email;
        user_json["full_name"] = full_name;
        user_json["role"] = "student";
        user_json["course"] = course;
        user_json["password_hash"] = password_hash;
        
        data["users"].push_back(user_json);
        saveDB();
        
        return new_user;
    }
    
    return User{};
}

// Метод уже должен быть в вашем файле:
User SimpleDB::createUserWithPassword(const string& username,
                                     const string& email,
                                     const string& full_name,
                                     const string& password_hash,
                                     const string& course,
                                     const string& role) {
    // Create new user without GitHub
    User new_user;
    new_user.id = generateId();
    new_user.github_id = "";
    new_user.username = username;
    new_user.email = email;
    new_user.full_name = full_name;
    new_user.role = role;
    new_user.course = course;
    new_user.password_hash = password_hash;
    
    json user_json;
    user_json["id"] = new_user.id;
    user_json["github_id"] = "";
    user_json["username"] = username;
    user_json["email"] = email;
    user_json["full_name"] = full_name;
    user_json["role"] = role;
    user_json["course"] = course;
    user_json["password_hash"] = password_hash;
    
    data["users"].push_back(user_json);
    saveDB();
    
    return new_user;
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
            u.course = user.value("course", "1");
            u.password_hash = user.value("password_hash", "");
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
            u.course = user.value("course", "1");
            u.password_hash = user.value("password_hash", "");
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
        u.course = user.value("course", "1");
        u.password_hash = user.value("password_hash", "");
        users.push_back(u);
    }
    
    return users;
}

void SimpleDB::saveRefreshToken(const string& user_id, const string& refresh_token) {
    json token;
    token["user_id"] = user_id;
    token["token"] = refresh_token;
    token["created_at"] = getCurrentTime();
    
    data["tokens"].push_back(token);
    saveDB();
}

bool SimpleDB::validateRefreshToken(const string& user_id, const string& refresh_token) {
    time_t now = getCurrentTime();
    
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