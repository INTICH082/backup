#pragma once
#include <string>
#include <cstdlib>

using namespace std;

namespace Config {
    // Функция для чтения переменных окружения
    inline string getEnv(const string& key, const string& defaultValue = "") {
        const char* val = getenv(key.c_str());
        return val ? string(val) : defaultValue;
    }
    
    inline int getEnvInt(const string& key, int defaultValue = 0) {
        const char* val = getenv(key.c_str());
        if (!val) return defaultValue;
        try {
            return stoi(string(val));
        } catch (...) {
            return defaultValue;
        }
    }

    // GitHub OAuth
    const string GITHUB_CLIENT_ID = getEnv("GITHUB_CLIENT_ID", "Ov23lisJdUcb1DmKhIfe");
    const string GITHUB_CLIENT_SECRET = getEnv("GITHUB_CLIENT_SECRET", "897dbebdde0fcb173d22f45f53de423bb7bb44ac");
    
    // Порт сервера
    const int PORT = getEnvInt("PORT", 8081);
    
    // JWT секрет
    const string JWT_SECRET = getEnv("JWT_SECRET", "iplaygodotandclaimfun");
    
    // Токены
    const int ACCESS_TOKEN_EXPIRE_SEC = 900;       // 15 минут
    const int REFRESH_TOKEN_EXPIRE_SEC = 2592000;  // 30 дней
}