#ifndef AUTH_SERVER_H
#define AUTH_SERVER_H

#include <string>
#include <functional>
#include <map>
#include <thread>
#include <vector>

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
    #define close closesocket
    #define SHUT_RDWR SD_BOTH
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <unistd.h>
    #include <arpa/inet.h>
#endif

using namespace std;

class AuthServer {
private:
    int server_port;
    string server_host;
    int server_socket;
    bool running;
    thread server_thread;
    
    map<string, 
        function<string(const map<string, string>&)>> routes;
    
    void handleClient(int client_socket);
    map<string, string> parseQueryParams(const string& query);
    string urlDecode(const string& str);
    
public:
    AuthServer(int port = 8081, const string& host = "localhost");
    ~AuthServer();
    
    void start();
    void stop();
    
    void addRoute(const string& path, 
                  function<string(const map<string, string>&)> handler);
    
    string sendJsonResponse(const string& json_data, 
                           int status_code = 200,
                           const string& content_type = "application/json");
    string sendErrorResponse(const string& message, int status_code = 400);
    string sendRedirect(const string& url);
    
    // Windows-specific initialization
    #ifdef _WIN32
    bool initWinsock();
    void cleanupWinsock();
    #endif
};

#endif