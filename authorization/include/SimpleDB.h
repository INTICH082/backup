#ifndef SIMPLEDB_H
#define SIMPLEDB_H

#include <string>
#include <vector>
#include <map>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>

using namespace std;
using json = nlohmann::json;

struct User {
    string id;
    string github_id;
    string username;
    string email;
    string full_name;
    string role;
};

class SimpleDB {
private:
    string db_file;
    json data;
    
    void loadDB();
    void saveDB();
    string generateId();
    
public:
    SimpleDB(const string& db_file = "auth_db.json");
    
    bool testConnection();
    void initializeDB();
    
    // User operations
    User createOrUpdateUser(const string& github_id,
                          const string& username,
                          const string& email,
                          const string& full_name);
    
    User getUserById(const string& user_id);
    User getUserByGithubId(const string& github_id);
    vector<User> getAllUsers();
    
    bool updateUserRole(const string& user_id, const string& role);
    
    // Token operations
    void saveRefreshToken(const string& user_id, const string& refresh_token);
    bool validateRefreshToken(const string& user_id, const string& refresh_token);
    void revokeRefreshToken(const string& user_id);
    
    // Session operations
    void createSession(const string& user_id, const string& session_token);
    bool validateSession(const string& session_token);
    void deleteSession(const string& session_token);
};

#endif