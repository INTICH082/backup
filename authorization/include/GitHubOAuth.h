#ifndef GITHUB_OAUTH_H
#define GITHUB_OAUTH_H

#include <string>
#include <map>

using namespace std;

struct GitHubUser {
    string id;
    string login;
    string name;
    string email;
};

class GitHubOAuth {
private:
    string client_id;
    string client_secret;
    string redirect_uri;
    
    string makeHttpRequest(const string& url, 
                          const map<string, string>& headers,
                          const string& post_data = "");
    
public:
    GitHubOAuth(const string& client_id, 
               const string& client_secret,
               const string& redirect_uri);
    
    string getAuthorizationUrl() const;
    string getAccessToken(const string& code);
    GitHubUser getUserInfo(const string& access_token);
    string getAuthorizationUrlWithToken(const string& login_token) const;
    void setRedirectUri(const string& new_uri);
};

#endif