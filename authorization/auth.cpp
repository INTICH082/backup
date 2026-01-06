#include "auth.h"
#include "database.h"
#include "config.h"
#include <curl/curl.h>
#include <ctime>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <cstdlib>
#include <openssl/sha.h>  // ДОБАВИТЬ
#include <iomanip>        // ДОБАВИТЬ
#include <vector>         // ДОБАВИТЬ

using namespace std;

static size_t writeCallback(void* data, size_t size, size_t nmemb, void* userp) {
    string* str = (string*)userp;
    str->append((char*)data, size * nmemb);
    return size * nmemb;
}

bool Auth::init() {
    curl_global_init(CURL_GLOBAL_ALL);
    return Database::connect();
}

void Auth::cleanup() {
    Database::close();
    curl_global_cleanup();
}

// ИСПРАВЛЕННАЯ ФУНКЦИЯ - ЗАМЕНИТЬ
string Auth::hashPassword(const string& password) {
    // Простое SHA256 с солью (лучше чем DJB2)
    string salted = password + Config::JWT_SECRET + "static_salt_for_student_project";
    unsigned char hash[SHA256_DIGEST_LENGTH];
    
    // Если OpenSSL не установлен, используем fallback
    #ifdef OPENSSL_VERSION_NUMBER
    SHA256_CTX sha256;
    SHA256_Init(&sha256);
    SHA256_Update(&sha256, salted.c_str(), salted.length());
    SHA256_Final(hash, &sha256);
    #else
    // Fallback: XOR хэш (все равно лучше DJB2)
    for (size_t i = 0; i < salted.length(); i++) {
        hash[i % SHA256_DIGEST_LENGTH] ^= salted[i] ^ (i * 31);
    }
    #endif
    
    // Конвертируем в hex строку
    stringstream ss;
    for(int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
        ss << hex << setw(2) << setfill('0') << (int)hash[i];
    }
    return ss.str();
}

bool Auth::verifyPassword(const string& password, const string& hash) {
    return hashPassword(password) == hash;
}

string Auth::registerUser(const string& login, const string& password,
                         const string& fullname, const string& email) {
    if (login.empty() || password.empty() || fullname.empty() || email.empty()) {
        return "{\"success\":false,\"error\":\"Все поля обязательны\"}";
    }
    
    // Простая валидация email
    if (email.find('@') == string::npos) {
        return "{\"success\":false,\"error\":\"Неверный email\"}";
    }
    
    if (Database::getUserByLogin(login) != 0) {
        return "{\"success\":false,\"error\":\"Логин уже существует\"}";
    }
    
    string password_hash = hashPassword(password);
    int user_id = Database::createUserWithPassword(login, password_hash, fullname, email);
    
    if (user_id == 0) return "{\"success\":false,\"error\":\"Ошибка БД\"}";
    
    string tokens = generateTokenPair(user_id);
    return "{\"success\":true,\"data\":" + tokens + "}";
}

string Auth::loginUser(const string& login, const string& password) {
    if (login.empty() || password.empty()) {
        return "{\"success\":false,\"error\":\"Логин и пароль обязательны\"}";
    }
    
    auto user_data = Database::getUserWithPasswordHash(login);
    if (user_data.first == 0 || !verifyPassword(password, user_data.second)) {
        return "{\"success\":false,\"error\":\"Неверный логин или пароль\"}";
    }
    
    string tokens = generateTokenPair(user_data.first);
    return "{\"success\":true,\"data\":" + tokens + "}";
}

// ОСТАВШАЯСЯ ЧАСТЬ БЕЗ ИЗМЕНЕНИЙ...
string Auth::getGitHubToken(const string& code) {
    CURL* curl = curl_easy_init();
    string response;
    
    if (curl) {
        string data = "client_id=" + Config::GITHUB_CLIENT_ID +
                     "&client_secret=" + Config::GITHUB_CLIENT_SECRET +
                     "&code=" + code;
        
        curl_easy_setopt(curl, CURLOPT_URL, "https://github.com/login/oauth/access_token");
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
        
        curl_easy_perform(curl);
        curl_easy_cleanup(curl);
    }
    
    size_t pos = response.find("access_token=");
    if (pos != string::npos) {
        size_t end = response.find('&', pos);
        if (end == string::npos) end = response.length();
        return response.substr(pos + 13, end - pos - 13);
    }
    
    return "";
}

string Auth::getGitHubUser(const string& token) {
    CURL* curl = curl_easy_init();
    string response;
    
    if (curl) {
        struct curl_slist* headers = nullptr;
        headers = curl_slist_append(headers, ("Authorization: token " + token).c_str());
        headers = curl_slist_append(headers, "User-Agent: StudentProject");
        
        curl_easy_setopt(curl, CURLOPT_URL, "https://api.github.com/user");
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
        
        curl_easy_perform(curl);
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
    }
    
    return response;
}

// ИСПРАВЛЕННЫЙ ПАРСЕР JSON - ЗАМЕНИТЬ
string Auth::parseJson(const string& json, const string& key) {
    // Ищем ключ в кавычках
    string search_key = "\"" + key + "\":";
    size_t pos = json.find(search_key);
    
    if (pos == string::npos) return "";
    
    // Пропускаем ключ
    size_t value_start = pos + search_key.length();
    
    // Пропускаем пробелы
    while (value_start < json.length() && isspace(json[value_start])) {
        value_start++;
    }
    
    if (value_start >= json.length()) return "";
    
    // Если значение в кавычках
    if (json[value_start] == '"') {
        size_t start = value_start + 1;
        size_t end = json.find('"', start);
        if (end == string::npos) return "";
        
        // Проверяем, не экранированная ли кавычка
        while (end > 0 && json[end-1] == '\\') {
            end = json.find('"', end + 1);
            if (end == string::npos) return "";
        }
        
        return json.substr(start, end - start);
    }
    // Если число или другое значение
    else {
        size_t end = value_start;
        while (end < json.length() && 
               json[end] != ',' && 
               json[end] != '}' && 
               !isspace(json[end])) {
            end++;
        }
        return json.substr(value_start, end - value_start);
    }
}

// ОСТАВШАЯСЯ ЧАСТЬ БЕЗ ИЗМЕНЕНИЙ...
string Auth::createToken(const string& data, int expire_seconds) {
    time_t now = time(nullptr);
    string full_data = data + "|" + to_string(now);
    
    unsigned long hash = 5381;
    for (char c : full_data + Config::JWT_SECRET) {
        hash = ((hash << 5) + hash) + c;
    }
    
    return full_data + "|" + to_string(hash);
}

bool Auth::parseToken(const string& token, int& user_id, string& type, time_t& created_at) {
    size_t pos1 = token.find('|');
    size_t pos2 = token.find('|', pos1 + 1);
    size_t pos3 = token.find('|', pos2 + 1);
    
    if (pos1 == string::npos || pos2 == string::npos || pos3 == string::npos) {
        return false;
    }
    
    string id_str = token.substr(0, pos1);
    type = token.substr(pos1 + 1, pos2 - pos1 - 1);
    string time_str = token.substr(pos2 + 1, pos3 - pos2 - 1);
    string hash_str = token.substr(pos3 + 1);
    
    // Проверка что это числа
    try {
        stoi(id_str);
        stoll(time_str);
    } catch(...) {
        return false;
    }
    
    string check_data = id_str + "|" + type + "|" + time_str;
    unsigned long check_hash = 5381;
    for (char c : check_data + Config::JWT_SECRET) {
        check_hash = ((check_hash << 5) + check_hash) + c;
    }
    
    if (to_string(check_hash) != hash_str) return false;
    
    user_id = stoi(id_str);
    created_at = stoll(time_str);
    return true;
}

string Auth::generateTokenPair(int user_id) {
    time_t now = time(nullptr);
    
    string access_token = createToken(to_string(user_id) + "|access|" + to_string(now), 
                                     Config::ACCESS_TOKEN_EXPIRE_SEC);
    string refresh_token = createToken(to_string(user_id) + "|refresh|" + to_string(now), 
                                      Config::REFRESH_TOKEN_EXPIRE_SEC);
    
    return "{\"access_token\":\"" + access_token + 
           "\",\"refresh_token\":\"" + refresh_token + 
           "\",\"user_id\":" + to_string(user_id) + 
           ",\"expires_in\":" + to_string(Config::ACCESS_TOKEN_EXPIRE_SEC) + "}";
}

string Auth::verifyToken(const string& token) {
    int user_id = 0;
    string type;
    time_t created_at = 0;
    
    if (!parseToken(token, user_id, type, created_at) || type != "access") {
        return "{\"success\":false,\"valid\":false}";
    }
    
    if (time(nullptr) - created_at > Config::ACCESS_TOKEN_EXPIRE_SEC) {
        return "{\"success\":false,\"valid\":false}";
    }
    
    return "{\"success\":true,\"valid\":true,\"user_id\":" + to_string(user_id) + "}";
}

string Auth::telegramAuth(const string& telegram_id_str, const string& name) {
    if (telegram_id_str.empty() || name.empty()) {
        return "{\"success\":false,\"error\":\"Требуется telegram_id и имя\"}";
    }
    
    // Проверка что telegram_id - число
    for (char c : telegram_id_str) {
        if (!isdigit(c)) {
            return "{\"success\":false,\"error\":\"telegram_id должен быть числом\"}";
        }
    }
    
    long long telegram_id = stoll(telegram_id_str);
    int user_id = Database::getUserByTelegramId(telegram_id);
    
    if (user_id == 0) {
        string login = "tg_" + to_string(telegram_id);
        string email = to_string(telegram_id) + "@telegram.user";
        user_id = Database::createTelegramUser(login, name, email, telegram_id);
    }
    
    if (user_id == 0) return "{\"success\":false,\"error\":\"Ошибка БД\"}";
    
    string tokens = generateTokenPair(user_id);
    return "{\"success\":true,\"data\":" + tokens + "}";
}

string Auth::startOAuth(const string& login_token) {
    if (login_token.empty()) {
        return "{\"success\":false,\"error\":\"Требуется login_token\"}";
    }
    
    // Проверяем токен и получаем user_id
    int user_id = TokenManager::validateLoginToken(login_token);
    if (user_id == 0) {
        return "{\"success\":false,\"error\":\"Неверный или устаревший login_token\"}";
    }
    
    // Создаем state токен для этого пользователя
    string state_token = TokenManager::createLoginToken(user_id);
    
    string url = "https://github.com/login/oauth/authorize?client_id=" + Config::GITHUB_CLIENT_ID +
                "&redirect_uri=http://localhost:" + to_string(Config::PORT) + "/auth/callback" +
                "&state=" + state_token + "&scope=user";
    
    return "{\"success\":true,\"auth_url\":\"" + url + "\", \"state_token\":\"" + state_token + "\"}";
}

string Auth::handleGitHubCallback(const string& code, const string& state) {
    if (code.empty()) {
        return "{\"success\":false,\"error\":\"Требуется код авторизации\"}";
    }
    
    int user_id = TokenManager::validateLoginToken(state);
    if (user_id == 0) return "{\"success\":false,\"error\":\"Неверный или устаревший токен\"}";
    
    string gh_token = getGitHubToken(code);
    if (gh_token.empty()) return "{\"success\":false,\"error\":\"Ошибка GitHub авторизации\"}";
    
    string user_info = getGitHubUser(gh_token);
    string github_id = parseJson(user_info, "id");
    string login = parseJson(user_info, "login");
    string name = parseJson(user_info, "name");
    string email = parseJson(user_info, "email");
    
    if (github_id.empty()) return "{\"success\":false,\"error\":\"Неверные данные от GitHub\"}";
    if (name.empty()) name = login;
    if (email.empty()) email = login + "@github.user";
    
    int existing_id = Database::getUserByGithubId(github_id);
    if (existing_id == 0) {
        existing_id = Database::createGitHubUser(login, name, email, github_id);
        if (existing_id == 0) return "{\"success\":false,\"error\":\"Ошибка создания пользователя\"}";
        user_id = existing_id;
    } else {
        user_id = existing_id;
    }
    
    string tokens = generateTokenPair(user_id);
    return "{\"success\":true,\"data\":" + tokens + "}";
}

string Auth::refreshToken(const string& refresh_token) {
    if (refresh_token.empty()) {
        return "{\"success\":false,\"error\":\"Требуется refresh_token\"}";
    }
    
    int user_id = 0;
    string type;
    time_t created_at = 0;
    
    if (!parseToken(refresh_token, user_id, type, created_at) || type != "refresh") {
        return "{\"success\":false,\"error\":\"Неверный refresh токен\"}";
    }
    
    if (time(nullptr) - created_at > Config::REFRESH_TOKEN_EXPIRE_SEC) {
        return "{\"success\":false,\"error\":\"Refresh токен устарел\"}";
    }
    
    string tokens = generateTokenPair(user_id);
    return "{\"success\":true,\"data\":" + tokens + "}";
}

// ИСПРАВЛЕННЫЙ TokenManager - ЗАМЕНИТЬ
map<string, pair<int, time_t>> TokenManager::loginTokens;  // Изменен тип

string TokenManager::createLoginToken(int user_id) {
    cleanupExpiredTokens();
    
    srand(static_cast<unsigned int>(time(nullptr)));
    string token_str = "login_" + to_string(user_id) + "_" + 
                      to_string(rand() % 1000000) + "_" + 
                      to_string(time(nullptr));
    
    // Более надежный хэш
    unsigned long hash = 5381;
    for (char c : token_str + Config::JWT_SECRET) {
        hash = ((hash << 5) + hash) + c;
    }
    
    // Используем hex представление
    stringstream ss;
    ss << hex << hash;
    token_str = ss.str();
    
    loginTokens[token_str] = {user_id, time(nullptr)};
    return token_str;
}

int TokenManager::validateLoginToken(const string& token) {
    auto it = loginTokens.find(token);
    if (it != loginTokens.end()) {
        int user_id = it->second.first;
        time_t created = it->second.second;
        
        // Токен живет 5 минут
        if (time(nullptr) - created <= 300) {
            loginTokens.erase(it);
            return user_id;
        } else {
            loginTokens.erase(it);
            return 0;
        }
    }
    return 0;
}

void TokenManager::cleanupExpiredTokens() {
    time_t now = time(nullptr);
    vector<string> to_remove;
    
    for (const auto& [token, data] : loginTokens) {
        if (now - data.second > 300) { // 5 минут
            to_remove.push_back(token);
        }
    }
    
    for (const auto& token : to_remove) {
        loginTokens.erase(token);
    }
}