#ifndef CONFIG_H
#define CONFIG_H

#include <string>
#include "json.hpp"  

using namespace std;

class Config {
private:
    string config_file;
    nlohmann::json data;  // Используем полное имя
    
    void loadConfig();
    void saveConfig();
    
public:
    Config(const string& config_file);
    
    string getString(const string& key, const string& default_value = "");
    int getInt(const string& key, int default_value = 0);
    bool getBool(const string& key, bool default_value = false);
    
    // Геттеры для конкретных настроек
    string getDbFile();
    string getJwtSecret();
    int getJwtExpiryHours();
    string getGithubClientId();
    string getGithubClientSecret();
    string getGithubRedirectUri();
};

#endif // CONFIG_H