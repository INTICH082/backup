#ifdef _WIN32
#undef byte
#define _NO_BYTE
#endif

#include "../include/Config.h"
#include <iostream>
#include "../include/precompiled.h"

Config::Config(const string& config_file) {
    ifstream file(config_file);
    if (!file.is_open()) {
        cerr << "Error: Could not open config file " << config_file << endl;
        exit(1);
    }
    
    try {
        file >> config;
    } catch (const json::parse_error& e) {
        cerr << "Error parsing config file: " << e.what() << endl;
        exit(1);
    }
}

string Config::getGithubClientId() const {
    return config["github"]["client_id"];
}

string Config::getGithubClientSecret() const {
    return config["github"]["client_secret"];
}

string Config::getGithubRedirectUri() const {
    return config["github"]["redirect_uri"];
}

string Config::getJwtSecret() const {
    return config["server"]["jwt_secret"];
}

int Config::getJwtExpiryHours() const {
    return config["server"]["jwt_expiry_hours"];
}

int Config::getServerPort() const {
    return config["server"]["port"];
}

string Config::getDbFile() const {
    return config["database"]["file"];
}