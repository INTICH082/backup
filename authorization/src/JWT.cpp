#include "../include/JWT.h"
#include <openssl/hmac.h>
#include <openssl/sha.h>
#include <sstream>
#include <iostream>
#include <cstring>
#include <iomanip>
#include <cctype>
#include "../include/precompiled.h"

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
    size_t in_len = input.size();
    int i = 0;
    int j = 0;
    int in_ = 0;
    unsigned char char_array_4[4], char_array_3[3];
    string ret;
    
    while (in_len-- && (input[in_] != '=') && (std::isalnum(static_cast<unsigned char>(input[in_])) || (input[in_] == '+') || (input[in_] == '/'))) {
        char_array_4[i++] = input[in_]; in_++;
        if (i == 4) {
            for (i = 0; i < 4; i++)
                char_array_4[i] = static_cast<unsigned char>(base64_chars.find(char_array_4[i]));
            
            char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
            char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
            char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];
            
            for (i = 0; (i < 3); i++)
                ret += char_array_3[i];
            i = 0;
        }
    }
    
    if (i) {
        for (j = i; j < 4; j++)
            char_array_4[j] = 0;
        
        for (j = 0; j < 4; j++)
            char_array_4[j] = static_cast<unsigned char>(base64_chars.find(char_array_4[j]));
        
        char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
        char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
        char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];
        
        for (j = 0; (j < i - 1); j++) ret += char_array_3[j];
    }
    
    return ret;
}

JWT::JWT(const string& secret, int expiry_hours) 
    : secret_key(secret), expiry_hours(expiry_hours) {}

string JWT::sign(const string& data) {
    unsigned char digest[32];
    HMAC(EVP_sha256(), 
         secret_key.c_str(), static_cast<int>(secret_key.length()),
         (unsigned char*)data.c_str(), static_cast<int>(data.length()),
         digest, NULL);
    
    return base64_encode(digest, 32);
}

bool JWT::verify(const string& token, const string& signature) {
    string expected = sign(token);
    return expected == signature;
}

string JWT::generateToken(const map<string, string>& payload) {
    // Header
    json header;
    header["alg"] = "HS256";
    header["typ"] = "JWT";
    string header_str = header.dump();
    string header_b64 = base64_encode((unsigned char*)header_str.c_str(), header_str.length());
    
    // Payload with expiry
    json payload_json;
    for (const auto& pair : payload) {
        payload_json[pair.first] = pair.second;
    }
    
    time_t now = time(nullptr);
    // Исправлено: использовать = вместо -=
    payload_json["exp"] = now + (expiry_hours * 3600);
    string payload_str = payload_json.dump();
    string payload_b64 = base64_encode((unsigned char*)payload_str.c_str(), payload_str.length());
    
    // Signature
    string data = header_b64 + "." + payload_b64;
    string signature = sign(data);
    
    return data + "." + signature;
}

map<string, string> JWT::validateToken(const string& token) {
    size_t dot1 = token.find('.');
    size_t dot2 = token.find('.', dot1 + 1);
    
    if (dot1 == string::npos || dot2 == string::npos) {
        return {};
    }
    
    string header_b64 = token.substr(0, dot1);
    string payload_b64 = token.substr(dot1 + 1, dot2 - dot1 - 1);
    string signature_b64 = token.substr(dot2 + 1);
    
    string data = header_b64 + "." + payload_b64;
    
    if (!verify(data, base64_decode(signature_b64))) {
        return {};
    }
    
    string payload_str = base64_decode(payload_b64);
    json payload_json;
    
    try {
        payload_json = json::parse(payload_str);
    } catch (...) {
        return {};
    }
    
    // Check expiry
    if (payload_json.contains("exp")) {
        time_t exp = payload_json["exp"];
        time_t now = time(nullptr);
        if (now > exp) {
            return {};
        }
    }
    
    map<string, string> claims;
    for (auto& item : payload_json.items()) {
        if (item.value().is_string()) {
            claims[item.key()] = item.value().get<string>();
        } else if (item.value().is_number_integer()) {
            // Для чисел (например, exp) преобразуем в строку
            claims[item.key()] = to_string(item.value().get<int64_t>());
        }
    }
    
    return claims;
}

string JWT::generateRefreshToken() {
    const char charset[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
    const int length = 40;
    
    string token;
    
    srand(static_cast<unsigned int>(time(nullptr)));
    for (int i = 0; i < length; ++i) {
        token += charset[rand() % (sizeof(charset) - 1)];
    }
    
    return token;
}