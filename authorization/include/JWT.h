#ifndef JWT_H
#define JWT_H

#include <string>
#include <map>
#include <ctime>
#include "json.hpp"  // Из папки include

using namespace std;
using json = nlohmann::json;

class JWT {
private:
    string secret_key;
    int expiry_hours;
    
    string base64_encode(const unsigned char* input, size_t length);
    string base64_decode(const string& input);
    string sign(const string& data);
    bool verify(const string& token, const string& signature);
    
public:
    JWT(const string& secret, int expiry_hours = 24);
    
    string generateToken(const map<string, string>& payload);
    map<string, string> validateToken(const string& token);
    string generateRefreshToken();
};

#endif