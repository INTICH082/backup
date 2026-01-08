#include "../include/JWT.h"
#include <openssl/hmac.h>
#include <openssl/sha.h>
#include <sstream>
#include <iostream>
#include <cstring>
#include <iomanip>
#include <cctype>
#include <algorithm>
#include "../include/precompiled.h"

using namespace std;

// Исправлено: убрать static (ошибка storage class)
const string base64_chars = 
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyz"
    "0123456789+/";

string JWT::base64_encode(const unsigned char* input, size_t length) {
    string ret;
    int i = 0;
    int j = 0;
    unsigned char char_array_3[3];
    unsigned char char_array_4[4];
    
    while (length--) {
        char_array_3[i++] = *(input++);
        if (i == 3) {
            char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
            char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
            char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
            char_array_4[3] = char_array_3[2] & 0x3f;
            
            for(i = 0; i < 4; i++)
                ret += base64_chars[char_array_4[i]];
            i = 0;
        }
    }
    
    if (i) {
        for(j = i; j < 3; j++)
            char_array_3[j] = '\0';
        
        char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
        char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
        char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
        char_array_4[3] = char_array_3[2] & 0x3f;
        
        for (j = 0; j < i + 1; j++)
            ret += base64_chars[char_array_4[j]];
        
        while(i++ < 3)
            ret += '=';
    }
    
    return ret;
}

string JWT::base64_decode(const string& input) {
    if (input.empty()) {
        return "";
    }
    
    string modified = input;
    
    // Заменяем URL-safe символы на стандартные Base64 для JWT
    for (char& c : modified) {
        if (c == '-') c = '+';
        if (c == '_') c = '/';
    }
    
    size_t in_len = modified.size();
    int i = 0;
    int j = 0;
    int in_ = 0;
    unsigned char char_array_4[4] = {0};
    unsigned char char_array_3[3] = {0};
    string ret;
    
    while (in_len-- && in_ < modified.size()) {
        char c = modified[in_];
        
        // Проверяем допустимые символы Base64
        if ((c >= 'A' && c <= 'Z') || 
            (c >= 'a' && c <= 'z') || 
            (c >= '0' && c <= '9') || 
            c == '+' || c == '/' || c == '=') {
            
            char_array_4[i++] = c;
            in_++;
            
            if (i == 4) {
                // Декодируем 4 символа в 3 байта
                for (i = 0; i < 4; i++) {
                    if (char_array_4[i] == '=') {
                        char_array_4[i] = 0;
                    } else {
                        size_t pos = base64_chars.find(char_array_4[i]);
                        if (pos == string::npos) {
                            // Недопустимый символ
                            return "";
                        }
                        char_array_4[i] = static_cast<unsigned char>(pos);
                    }
                }
                
                char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
                char_array_3[1] = ((char_array_4[1] & 0x0f) << 4) + ((char_array_4[2] & 0x3c) >> 2);
                char_array_3[2] = ((char_array_4[2] & 0x03) << 6) + char_array_4[3];
                
                for (i = 0; i < 3; i++) {
                    ret += char_array_3[i];
                }
                i = 0;
            }
        } else {
            // Недопустимый символ
            return "";
        }
    }
    
    // Обработка оставшихся символов
    if (i > 0) {
        for (j = i; j < 4; j++) {
            char_array_4[j] = 0;
        }
        
        for (j = 0; j < 4; j++) {
            if (char_array_4[j] == '=') {
                char_array_4[j] = 0;
            } else if (char_array_4[j] != 0) {
                size_t pos = base64_chars.find(char_array_4[j]);
                if (pos == string::npos) {
                    return "";
                }
                char_array_4[j] = static_cast<unsigned char>(pos);
            }
        }
        
        char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
        char_array_3[1] = ((char_array_4[1] & 0x0f) << 4) + ((char_array_4[2] & 0x3c) >> 2);
        char_array_3[2] = ((char_array_4[2] & 0x03) << 6) + char_array_4[3];
        
        for (j = 0; j < i - 1; j++) {
            ret += char_array_3[j];
        }
    }
    
    return ret;
}

JWT::JWT(const string& secret, int expiry_hours) 
    : secret_key(secret), expiry_hours(expiry_hours) {
    // Логирование для отладки
    cout << "[JWT] Initialized with expiry: " << expiry_hours << " hours" << endl;
}

string JWT::sign(const string& data) {
    if (data.empty() || secret_key.empty()) {
        cerr << "[JWT] Cannot sign empty data or with empty secret key" << endl;
        return "";
    }
    
    cout << "[JWT] Signing data of length: " << data.length() << endl;
    
    unsigned char digest[EVP_MAX_MD_SIZE];
    unsigned int digest_len = 0;
    
    HMAC(EVP_sha256(), 
         secret_key.c_str(), static_cast<int>(secret_key.length()),
         reinterpret_cast<const unsigned char*>(data.c_str()), static_cast<int>(data.length()),
         digest, &digest_len);
    
    if (digest_len == 0) {
        cerr << "[JWT] HMAC failed to produce signature" << endl;
        return "";
    }
    
    cout << "[JWT] Generated signature of length: " << digest_len << " bytes" << endl;
    
    string signature_b64 = base64_encode(digest, digest_len);
    
    // JWT требует URL-safe Base64 без pad символов
    for (char& c : signature_b64) {
        if (c == '+') c = '-';
        if (c == '/') c = '_';
    }
    
    // Удаляем pad символы
    while (!signature_b64.empty() && signature_b64.back() == '=') {
        signature_b64.pop_back();
    }
    
    cout << "[JWT] Final signature B64: " << signature_b64 << endl;
    
    return signature_b64;
}

bool JWT::verify(const string& token, const string& signature) {
    if (token.empty() || signature.empty()) {
        cout << "[JWT] Cannot verify empty token or signature" << endl;
        return false;
    }
    
    string expected = sign(token);
    if (expected.empty()) {
        cout << "[JWT] Failed to generate signature for verification" << endl;
        return false;
    }
    
    // Декодируем обе сигнатуры для сравнения
    string expected_decoded = base64_decode(expected);
    string signature_decoded = base64_decode(signature);
    
    if (expected_decoded.empty() || signature_decoded.empty()) {
        cout << "[JWT] Failed to decode signatures for comparison" << endl;
        cout << "[JWT] Expected B64: " << expected << endl;
        cout << "[JWT] Signature B64: " << signature << endl;
        return false;
    }
    
    // Сравнение raw байтов
    bool result = (expected_decoded == signature_decoded);
    
    if (!result) {
        cout << "[JWT] Signature verification failed" << endl;
        cout << "[JWT] Expected length: " << expected_decoded.length() << endl;
        cout << "[JWT] Got length: " << signature_decoded.length() << endl;
        
        // Логирование для отладки (первые 16 байт)
        cout << "[JWT] Expected (first 16 bytes): ";
        for (size_t i = 0; i < min((size_t)16, expected_decoded.length()); i++) {
            printf("%02x ", (unsigned char)expected_decoded[i]);
        }
        cout << endl;
        
        cout << "[JWT] Got (first 16 bytes): ";
        for (size_t i = 0; i < min((size_t)16, signature_decoded.length()); i++) {
            printf("%02x ", (unsigned char)signature_decoded[i]);
        }
        cout << endl;
    } else {
        cout << "[JWT] Signature verification successful" << endl;
    }
    
    return result;
}

string JWT::generateToken(const map<string, string>& payload) {
    if (payload.empty()) {
        cerr << "[JWT] Cannot generate token with empty payload" << endl;
        return "";
    }
    
    // Header
    json header;
    header["alg"] = "HS256";
    header["typ"] = "JWT";
    string header_str = header.dump();
    string header_b64 = base64_encode(reinterpret_cast<const unsigned char*>(header_str.c_str()), header_str.length());
    
    // Заменяем стандартные Base64 символы на URL-safe для JWT
    for (char& c : header_b64) {
        if (c == '+') c = '-';
        if (c == '/') c = '_';
    }
    // Удаляем pad символы (=) для JWT
    while (!header_b64.empty() && header_b64.back() == '=') {
        header_b64.pop_back();
    }
    
    // Payload with expiry
    json payload_json;
    for (const auto& pair : payload) {
        payload_json[pair.first] = pair.second;
    }
    
    time_t now = time(nullptr);
    payload_json["exp"] = now + (expiry_hours * 3600);
    payload_json["iat"] = now; // Issued at timestamp
    
    string payload_str = payload_json.dump();
    string payload_b64 = base64_encode(reinterpret_cast<const unsigned char*>(payload_str.c_str()), payload_str.length());
    
    // Заменяем стандартные Base64 символы на URL-safe для JWT
    for (char& c : payload_b64) {
        if (c == '+') c = '-';
        if (c == '/') c = '_';
    }
    // Удаляем pad символы (=) для JWT
    while (!payload_b64.empty() && payload_b64.back() == '=') {
        payload_b64.pop_back();
    }
    
    // Signature
    string data = header_b64 + "." + payload_b64;
    cout << "[JWT] Data to sign: " << data << endl;
    
    string signature = sign(data);
    if (signature.empty()) {
        cerr << "[JWT] Failed to generate signature for token" << endl;
        return "";
    }
    
    string token = data + "." + signature;
    
    cout << "[JWT] Generated token with expiry: " << expiry_hours << " hours" << endl;
    cout << "[JWT] Token: " << token << endl;
    
    return token;
}

map<string, string> JWT::validateToken(const string& token) {
    cout << "[JWT] Validating token: " << (token.length() > 50 ? token.substr(0, 50) + "..." : token) << endl;
    
    if (token.empty()) {
        cout << "[JWT] Empty token provided" << endl;
        return {};
    }
    
    // Проверка формата токена (должен быть header.payload.signature)
    size_t dot1 = token.find('.');
    size_t dot2 = token.find('.', dot1 + 1);
    
    if (dot1 == string::npos || dot2 == string::npos || token.find('.', dot2 + 1) != string::npos) {
        cout << "[JWT] Invalid token format (expected header.payload.signature)" << endl;
        cout << "[JWT] dot1: " << dot1 << ", dot2: " << dot2 << endl;
        return {};
    }
    
    string header_b64 = token.substr(0, dot1);
    string payload_b64 = token.substr(dot1 + 1, dot2 - dot1 - 1);
    string signature_b64 = token.substr(dot2 + 1);
    
    cout << "[JWT] Header B64: " << header_b64 << endl;
    cout << "[JWT] Payload B64: " << payload_b64 << endl;
    cout << "[JWT] Signature B64: " << signature_b64 << endl;
    
    // Проверка подписи
    string data = header_b64 + "." + payload_b64;
    cout << "[JWT] Data to verify: " << data << endl;
    
    // Декодируем сигнатуру для проверки
    string decoded_signature = base64_decode(signature_b64);
    if (decoded_signature.empty()) {
        cout << "[JWT] Failed to decode signature" << endl;
        cout << "[JWT] Signature B64 that failed: " << signature_b64 << endl;
        return {};
    }
    
    cout << "[JWT] Decoded signature length: " << decoded_signature.length() << " bytes" << endl;
    
    if (!verify(data, decoded_signature)) {
        cout << "[JWT] Token signature verification failed" << endl;
        return {};
    }
    
    cout << "[JWT] Signature verification successful" << endl;
    
    // Декодирование payload
    string payload_str = base64_decode(payload_b64);
    if (payload_str.empty()) {
        cout << "[JWT] Failed to decode payload" << endl;
        cout << "[JWT] Payload B64 that failed: " << payload_b64 << endl;
        return {};
    }
    
    cout << "[JWT] Decoded payload: " << payload_str << endl;
    
    json payload_json;
    try {
        payload_json = json::parse(payload_str);
    } catch (const json::exception& e) {
        cout << "[JWT] JSON parse error: " << e.what() << endl;
        return {};
    }
    
    // Проверка обязательных полей
    if (!payload_json.contains("exp")) {
        cout << "[JWT] Token missing expiry (exp) field" << endl;
        return {};
    }
    
    // Проверка срока действия
    if (!payload_json["exp"].is_number()) {
        cout << "[JWT] exp field is not a number" << endl;
        return {};
    }
    
    time_t exp;
    try {
        exp = payload_json["exp"].get<time_t>();
    } catch (...) {
        cout << "[JWT] Failed to parse exp value" << endl;
        return {};
    }
    
    time_t now = time(nullptr);
    if (now > exp) {
        cout << "[JWT] Token expired at " << exp << ", now is " << now << endl;
        return {};
    }
    
    cout << "[JWT] Token expiry check passed. Exp: " << exp << ", Now: " << now << endl;
    
    // Преобразование claims в map
    map<string, string> claims;
    for (auto& item : payload_json.items()) {
        const string& key = item.key();
        
        try {
            if (item.value().is_string()) {
                claims[key] = item.value().get<string>();
            } else if (item.value().is_number_integer()) {
                claims[key] = to_string(item.value().get<int64_t>());
            } else if (item.value().is_number_float()) {
                claims[key] = to_string(item.value().get<double>());
            } else if (item.value().is_boolean()) {
                claims[key] = item.value().get<bool>() ? "true" : "false";
            } else if (item.value().is_null()) {
                claims[key] = "null";
            }
            // Пропускаем объекты и массивы
        } catch (const exception& e) {
            cout << "[JWT] Warning: Failed to process claim '" << key << "': " << e.what() << endl;
            continue;
        }
    }
    
    cout << "[JWT] Token validated successfully for user: " 
         << (claims.count("user_id") ? claims["user_id"] : "unknown") << endl;
    
    return claims;
}

string JWT::generateRefreshToken() {
    const char charset[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
    const int length = 40;
    
    string token;
    
    // Используем time для seed, но в реальном приложении лучше использовать более безопасный RNG
    srand(static_cast<unsigned int>(time(nullptr) + rand()));
    
    for (int i = 0; i < length; ++i) {
        token += charset[rand() % (sizeof(charset) - 1)];
    }
    
    cout << "[JWT] Generated refresh token" << endl;
    return token;
}