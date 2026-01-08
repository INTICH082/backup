// AuthSessionManager.cpp
#include "../include/AutoSessionManager.h"
#include <iostream>
#include <chrono>
#include <algorithm>

AuthSessionManager::AuthSessionManager() {
    // Запускаем очистку устаревших сессий при создании
    cleanupExpired();
}

string AuthSessionManager::generateToken() {
    const string charset = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
    const int token_length = 32;
    
    random_device rd;
    mt19937 generator(rd());
    uniform_int_distribution<> distribution(0, charset.size() - 1);
    
    string token = "token_";
    for (int i = 0; i < token_length; ++i) {
        token += charset[distribution(generator)];
    }
    
    return token;
}

void AuthSessionManager::cleanupExpired() {
    lock_guard<mutex> lock(sessions_mutex);
    time_t now = time(nullptr);
    
    vector<string> to_remove;
    
    for (const auto& pair : sessions) {
        if (pair.second.expires_at < now) {
            to_remove.push_back(pair.first);
        }
    }
    
    for (const auto& token : to_remove) {
        sessions.erase(token);
    }
    
    if (!to_remove.empty()) {
        cout << "[SessionManager] Cleaned up " << to_remove.size() << " expired sessions" << endl;
    }
}

string AuthSessionManager::createSession() {
    cleanupExpired();
    lock_guard<mutex> lock(sessions_mutex);
    
    string token = generateToken();
    time_t now = time(nullptr);
    
    AuthSession session;
    session.token = token;
    session.status = "pending";
    session.created_at = now;
    session.expires_at = now + 300; // 5 минут
    
    sessions[token] = session;
    
    cout << "[SessionManager] Created session: " << token << endl;
    return token;
}

bool AuthSessionManager::updateSessionWithCode(const string& token, const string& github_code) {
    lock_guard<mutex> lock(sessions_mutex);
    
    if (sessions.find(token) == sessions.end()) {
        return false;
    }
    
    AuthSession& session = sessions[token];
    
    // Проверяем не истекла ли сессия
    if (session.expires_at < time(nullptr)) {
        session.status = "expired";
        return false;
    }
    
    session.github_code = github_code;
    cout << "[SessionManager] Updated session " << token << " with GitHub code" << endl;
    
    return true;
}

bool AuthSessionManager::setSessionSuccess(const string& token, 
                                         const string& access_token, 
                                         const string& refresh_token,
                                         const string& user_id,
                                         const string& github_id) {
    lock_guard<mutex> lock(sessions_mutex);
    
    if (sessions.find(token) == sessions.end()) {
        return false;
    }
    
    AuthSession& session = sessions[token];
    
    // Проверяем не истекла ли сессия
    if (session.expires_at < time(nullptr)) {
        session.status = "expired";
        return false;
    }
    
    session.status = "success";
    session.access_token = access_token;
    session.refresh_token = refresh_token;
    session.user_id = user_id;
    session.github_id = github_id;
    
    // Увеличиваем время жизни при успехе
    session.expires_at = time(nullptr) + 600; // 10 минут
    
    cout << "[SessionManager] Session " << token << " marked as success for user " << user_id << endl;
    return true;
}

bool AuthSessionManager::setSessionDenied(const string& token) {
    lock_guard<mutex> lock(sessions_mutex);
    
    if (sessions.find(token) == sessions.end()) {
        return false;
    }
    
    sessions[token].status = "denied";
    cout << "[SessionManager] Session " << token << " marked as denied" << endl;
    return true;
}

AuthSession AuthSessionManager::getSession(const string& token) {
    lock_guard<mutex> lock(sessions_mutex);
    
    if (sessions.find(token) == sessions.end()) {
        return AuthSession{}; // Пустая структура
    }
    
    // Проверяем не истекла ли сессия
    AuthSession& session = sessions[token];
    if (session.expires_at < time(nullptr) && session.status == "pending") {
        session.status = "expired";
    }
    
    return session;
}

bool AuthSessionManager::removeSession(const string& token) {
    lock_guard<mutex> lock(sessions_mutex);
    
    auto it = sessions.find(token);
    if (it != sessions.end()) {
        sessions.erase(it);
        cout << "[SessionManager] Removed session: " << token << endl;
        return true;
    }
    
    return false;
}

vector<AuthSession> AuthSessionManager::getAllSessions() {
    lock_guard<mutex> lock(sessions_mutex);
    vector<AuthSession> result;
    
    for (const auto& pair : sessions) {
        result.push_back(pair.second);
    }
    
    return result;
}