#ifndef JWT_H
#define JWT_H

#include <string>
#include <map>

using namespace std;

class JWT {
private:
    string secret_key;
    int expiry_hours;
    
    string base64_encode(const string& input);
    string base64_decode(const string& input);
    string sign(const string& header, const string& payload);
    bool verify(const string& token);
    
public:
    JWT(const string& secret, int expiry_hours = 24);
    
    string generateToken(const map<string, string>& payload);
    map<string, string> validateToken(const string& token);
    map<string, string> decodeToken(const string& token);
    
    string generateRefreshToken();
};

#endif