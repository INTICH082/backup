#include "../include/JWT.h"
#include <openssl/hmac.h>
#include <openssl/sha.h>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <iostream>
#include <cstring>

static const string base64_chars = 
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyz"
    "0123456789+/";

string JWT::base64_encode(const string& input) {
    string ret;
    int i = 0;
    int j = 0;
    unsigned char char_array_3[3];
    unsigned char char_array_4[4];
    size_t in_len = input.size();
    const char* bytes_to_encode = input.c_str();
    
    while (in_len--) {
        char_array_3[i++] = *(bytes_to_encode++);
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
    
    while (in_len-- && (input[in_] != '=') && isalnum(input[in_]) || (input[in_] == '+') || (input[in_] == '/')) {
        char_array_4[i++] = input[in_]; in_++;
        if (i == 4) {
            for (i = 0; i <4; i++)
                char_array_4[i] = base64_chars.find(char_array_4[i]);
            
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
            char_array_4[j] = base64_chars.find(char_array_4[j]);
        
        char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
        char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
        char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];
        
        for (j = 0; (j < i - 1); j++) ret += char_array_3[j];
    }
    
    return ret;
}

JWT::JWT(const string& secret, int expiry_hours) 
    : secret_key(secret), expiry_hours(expiry_hours) {}

string JWT::sign(const string& header, const string& payload) {
    string data = base64_encode(header) + "." + base64_encode(payload);
    
    unsigned char* digest = HMAC(EVP_sha256(), 
                                 secret_key.c_str(), 
                                 secret_key.length(),
                                 (unsigned char*)data.c_str(), 
                                 data.length(),
                                 NULL, NULL);
    
    string signature;
    for(int i = 0; i < 32; i++) {
        char buf[3];
        sprintf(buf, "%02x", digest[i]);
        signature += buf;
    }
    
    return base64_encode(signature);
}

bool JWT::verify(const string& token) {
    size_t dot1 = token.find('.');
    size_t dot2 = token.find('.', dot1 + 1);
    
    if (dot1 == string::npos || dot2 == string::npos) {
        return false;
    }
    
    string header_b64 = token.substr(0, dot1);
    string payload_b64 = token.substr(dot1 + 1, dot2 - dot1 - 1);
    string signature_b64 = token.substr(dot2 + 1);
    
    string data = header_b64 + "." + payload_b64;
    string expected_signature = sign(base64_decode(header_b64), 
                                    base64_decode(payload_b64));
    
    return base64_encode(expected_signature) == signature_b64;
}

string JWT::generateToken(const map<string, string>& payload) {
    string header = "{\"alg\":\"HS256\",\"typ\":\"JWT\"}";
    
    stringstream payload_ss;
    payload_ss << "{";
    bool first = true;
    
    time_t now = time(nullptr);
    time_t expiry = now + (expiry_hours * 3600);
    payload_ss << "\"exp\":" << expiry;
    
    for (const auto& item : payload) {
        if (!first) payload_ss << ",";
        first = false;
        payload_ss << "\"" << item.first << "\":\"" << item.second << "\"";
    }
    payload_ss << "}";
    
    string signature = sign(header, payload_ss.str());
    
    return base64_encode(header) + "." + 
           base64_encode(payload_ss.str()) + "." + 
           base64_encode(signature);
}

map<string, string> JWT::validateToken(const string& token) {
    if (!verify(token)) {
        return {};
    }
    
    map<string, string> claims = decodeToken(token);
    
    if (claims.find("exp") != claims.end()) {
        time_t exp = stol(claims["exp"]);
        time_t now = time(nullptr);
        
        if (now > exp) {
            return {};
        }
    }
    
    return claims;
}

map<string, string> JWT::decodeToken(const string& token) {
    size_t dot1 = token.find('.');
    size_t dot2 = token.find('.', dot1 + 1);
    
    if (dot1 == string::npos || dot2 == string::npos) {
        return {};
    }
    
    string payload_b64 = token.substr(dot1 + 1, dot2 - dot1 - 1);
    string payload = base64_decode(payload_b64);
    
    map<string, string> claims;
    
    size_t pos = 0;
    while (pos < payload.length()) {
        size_t key_start = payload.find('"', pos);
        if (key_start == string::npos) break;
        
        size_t key_end = payload.find('"', key_start + 1);
        if (key_end == string::npos) break;
        
        string key = payload.substr(key_start + 1, key_end - key_start - 1);
        
        size_t value_start = payload.find('"', key_end + 1);
        if (value_start == string::npos) break;
        
        size_t value_end = payload.find('"', value_start + 1);
        if (value_end == string::npos) break;
        
        string value = payload.substr(value_start + 1, value_end - value_start - 1);
        
        claims[key] = value;
        pos = value_end + 1;
    }
    
    return claims;
}

string JWT::generateRefreshToken() {
    const char charset[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
    const int length = 64;
    
    string token;
    token.reserve(length);
    
    srand(time(nullptr));
    for (int i = 0; i < length; ++i) {
        token += charset[rand() % (sizeof(charset) - 1)];
    }
    
    return token;
}