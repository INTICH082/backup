#ifndef SIMPLEDB_H
#define SIMPLEDB_H

#include <string>
#include <vector>
#include <map>
#include <fstream>
#include <iostream>
#include "json.hpp"

using namespace std;
using json = nlohmann::json;

struct User {
    string id;
    string github_id;
    string username;
    string email;
    string full_name;
    string role;
    string course;       // Добавлено
    string password_hash; // Добавлено (не password, а hash)
};

class SimpleDB {
private:
    string db_file;
    json data;
    
    void loadDB();
    void saveDB();
    string generateId();
    time_t getCurrentTime();
    
public:
    SimpleDB(const string& db_file = "users_db.json");
    
    void initializeDB();
    
    // User operations
    User createOrUpdateUser(const string& github_id,
                          const string& username,
                          const string& email,
                          const string& full_name,
                          const string& course = "1",
                          const string& password_hash = "");
    
    User getUserById(const string& user_id);
    User getUserByGithubId(const string& github_id);
    vector<User> getAllUsers();
    
    // Token operations
    void saveRefreshToken(const string& user_id, const string& refresh_token);
    bool validateRefreshToken(const string& user_id, const string& refresh_token);
    void revokeRefreshToken(const string& user_id);
    
    // Новые методы
    User createUserWithPassword(const string& username,
                               const string& email,
                               const string& full_name,
                               const string& password_hash,
                               const string& course = "1",
                               const string& role = "student");
};

#endif