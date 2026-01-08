#include "../include/Config.h"
#include "../include/precompiled.h"
#include <fstream>
#include <sstream>

Config::Config(const string& filename) : config_file(filename) {
    loadConfig();
}

void Config::loadConfig() {
    ifstream file(config_file);
    if (file.is_open()) {
        try {
            stringstream buffer;
            buffer << file.rdbuf();
            string json_str = buffer.str();
            
            if (!json_str.empty()) {
                data = json::parse(json_str);
            } else {
                data = json::object();
            }
        } catch (...) {
            data = json::object();
        }
        file.close();
    } else {
        data = json::object();
        saveConfig();
    }
}

void Config::saveConfig() {
    ofstream file(config_file);
    if (file.is_open()) {
        file << data.dump(4);
        file.close();
    }
}

string Config::getString(const string& key, const string& default_value) {
    if (data.contains(key)) {
        return data[key].get<string>();
    }
    return default_value;
}

int Config::getInt(const string& key, int default_value) {
    if (data.contains(key)) {
        return data[key].get<int>();
    }
    return default_value;
}

bool Config::getBool(const string& key, bool default_value) {
    if (data.contains(key)) {
        return data[key].get<bool>();
    }
    return default_value;
}

string Config::getDbFile() {
    return getString("db_file", "users.json");
}

string Config::getJwtSecret() {
    return getString("jwt_secret", "your-secret-key-change-this");
}

int Config::getJwtExpiryHours() {
    return getInt("jwt_expiry_hours", 24);
}

string Config::getGithubClientId() {
    return getString("github_client_id", "");
}

string Config::getGithubClientSecret() {
    return getString("github_client_secret", "");
}

string Config::getGithubRedirectUri() {
    return getString("github_redirect_uri", "auto");
}