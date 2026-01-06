#pragma once
#include <string>
#include <cstdlib>
#include <stdexcept>  // ДОБАВИТЬ

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

    // GitHub OAuth - НИКОГДА не хардкодить в коде!
    // Используем .env файл
    const string GITHUB_CLIENT_ID = getEnv("GITHUB_CLIENT_ID");
    const string GITHUB_CLIENT_SECRET = getEnv("GITHUB_CLIENT_SECRET");
    
    // Порт сервера
    const int PORT = getEnvInt("PORT", 8081);
    
    // JWT секрет - ДОЛЖЕН быть в .env!
    const string JWT_SECRET = getEnv("JWT_SECRET", "change_this_in_production");
    
    // Проверка конфигурации
    inline void validateConfig() {
        if (JWT_SECRET == "change_this_in_production") {
            cerr << "⚠️  ВНИМАНИЕ: Используется дефолтный JWT_SECRET!" << endl;
            cerr << "   Установите переменную окружения JWT_SECRET" << endl;
        }
        
        if (GITHUB_CLIENT_ID.empty() || GITHUB_CLIENT_SECRET.empty()) {
            cerr << "⚠️  ВНИМАНИЕ: GitHub OAuth не настроен" << endl;
        }
    }
    
    // Токены
    const int ACCESS_TOKEN_EXPIRE_SEC = 900;       // 15 минут
    const int REFRESH_TOKEN_EXPIRE_SEC = 2592000;  // 30 дней
}