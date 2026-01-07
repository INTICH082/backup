#ifndef CONFIG_H
#define CONFIG_H

#include <string>
#include <fstream>
#include <nlohmann/json.hpp>

using namespace std;
using json = nlohmann::json;

class Config {
private:
    json config;
    
public:
    Config(const string& config_file = "config.json");
    
    // GitHub OAuth config
    string getGithubClientId() const;
    string getGithubClientSecret() const;
    string getGithubRedirectUri() const;
    
    // JWT config
    string getJwtSecret() const;
    int getJwtExpiryHours() const;
    
    // Server config
    int getServerPort() const;
    string getServerHost() const;
    
    // Database config (файловое хранилище)
    string getDbFile() const;
};

#endif