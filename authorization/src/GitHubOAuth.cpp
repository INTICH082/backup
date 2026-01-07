// CRITICAL FIX: Prevent Windows byte conflict
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
// Prevent std::byte inclusion
#define _HAS_STD_BYTE 0
#endif

// Добавьте эти строки ПЕРЕД #include <curl/curl.h>
#include <winsock2.h>
#include <windows.h>
#undef byte  // Удаляем Windows byte definition

#include "../include/GitHubOAuth.h"
#include <curl/curl.h>
#include <sstream>
#include <iostream>
#include <cstring>
#include "../include/precompiled.h"

static size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    ((string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

GitHubOAuth::GitHubOAuth(const string& client_id,
                         const string& client_secret,
                         const string& redirect_uri)
    : client_id(client_id), client_secret(client_secret), redirect_uri(redirect_uri) {}

string GitHubOAuth::getAuthorizationUrl() const {
    return "https://github.com/login/oauth/authorize?client_id=" + client_id +
           "&redirect_uri=" + redirect_uri +
           "&scope=user:email";
}

string GitHubOAuth::makeHttpRequest(const string& url,
                                   const map<string, string>& headers,
                                   const string& post_data) {
    CURL* curl = curl_easy_init();
    string response_string;
    
    if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_string);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "Student-Auth-Server/1.0");
        
        if (!post_data.empty()) {
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_data.c_str());
        }
        
        struct curl_slist* header_list = nullptr;
        for (const auto& header : headers) {
            string header_str = header.first + ": " + header.second;
            header_list = curl_slist_append(header_list, header_str.c_str());
        }
        if (header_list) {
            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, header_list);
        }
        
        CURLcode res = curl_easy_perform(curl);
        if (res != CURLE_OK) {
            cerr << "HTTP request failed: " << curl_easy_strerror(res) << endl;
        }
        
        if (header_list) {
            curl_slist_free_all(header_list);
        }
        curl_easy_cleanup(curl);
    }
    
    return response_string;
}

string GitHubOAuth::getAccessToken(const string& code) {
    string token_url = "https://github.com/login/oauth/access_token";
    
    map<string, string> headers = {
        {"Accept", "application/json"},
        {"Content-Type", "application/x-www-form-urlencoded"}
    };
    
    string post_data = "client_id=" + client_id +
                      "&client_secret=" + client_secret +
                      "&code=" + code +
                      "&redirect_uri=" + redirect_uri;
    
    string response = makeHttpRequest(token_url, headers, post_data);
    
    // Simple JSON parsing
    size_t token_pos = response.find("\"access_token\":\"");
    if (token_pos != string::npos) {
        token_pos += 16;
        size_t token_end = response.find("\"", token_pos);
        if (token_end != string::npos) {
            return response.substr(token_pos, token_end - token_pos);
        }
    }
    
    return "";
}

GitHubUser GitHubOAuth::getUserInfo(const string& access_token) {
    string user_url = "https://api.github.com/user";
    
    map<string, string> headers = {
        {"Authorization", "token " + access_token},
        {"Accept", "application/json"},
        {"User-Agent", "Student-Auth-Server"}
    };
    
    string response = makeHttpRequest(user_url, headers);
    
    GitHubUser user;
    
    // Extract fields from JSON response
    auto extract = [&](const string& field) -> string {
        size_t pos = response.find("\"" + field + "\":");
        if (pos == string::npos) return "";
        pos += field.length() + 3;
        size_t end = response.find_first_of(",\"}", pos);
        if (end == string::npos) return "";
        string val = response.substr(pos, end - pos);
        if (val.size() >= 2 && val[0] == '"' && val.back() == '"') {
            val = val.substr(1, val.length() - 2);
        }
        return val;
    };
    
    user.id = extract("id");
    user.login = extract("login");
    user.name = extract("name");
    user.email = extract("email");
    
    return user;
}