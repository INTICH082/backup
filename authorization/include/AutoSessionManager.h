// AuthSessionManager.h
#ifndef AUTH_SESSION_MANAGER_H
#define AUTH_SESSION_MANAGER_H

#include <string>
#include <map>
#include <ctime>
#include <mutex>
#include <random>
#include "json.hpp"

using namespace std;
using json = nlohmann::json;

struct AuthSession {
    string token;           // login_token (формат: token_abc123)
    string status;          // "pending", "success", "denied", "expired"
    string github_code;     // Код от GitHub OAuth
    string access_token;    // JWT Access Token
    string refresh_token;   // JWT Refresh Token
    string user_id;         // ID пользователя из базы
    string github_id;       // GitHub ID пользователя
    time_t created_at;      // Время создания
    time_t expires_at;      // Время истечения (5 минут)
};

class AuthSessionManager {
private:
    map<string, AuthSession> sessions;
    mutex sessions_mutex;
    
    string generateToken();
    void cleanupExpired();
    
public:
    AuthSessionManager();
    
    // Создание новой сессии для авторизации
    string createSession();
    
    // Обновление сессии после получения кода от GitHub
    bool updateSessionWithCode(const string& token, const string& github_code);
    
    // Установка успешной авторизации
    bool setSessionSuccess(const string& token, 
                          const string& access_token, 
                          const string& refresh_token,
                          const string& user_id,
                          const string& github_id);
    
    // Установка отказа в авторизации
    bool setSessionDenied(const string& token);
    
    // Получение информации о сессии
    AuthSession getSession(const string& token);
    
    // Удаление сессии
    bool removeSession(const string& token);
    
    // Получение всех сессий (для отладки)
    vector<AuthSession> getAllSessions();
};

#endif