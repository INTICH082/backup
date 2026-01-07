#include "../include/AuthServer.h"
#include <iostream>
#include <sstream>
#include <cstring>
#include <algorithm>
#include <cctype>

#ifdef _WIN32
    #include <windows.h>
    #define SOCKET_ERROR_CODE WSAGetLastError()
#else
    #define SOCKET_ERROR_CODE errno
#endif

using namespace std;

AuthServer::AuthServer(int port, const string& host) 
    : server_port(port), server_host(host), server_socket(-1), running(false) {
    
    #ifdef _WIN32
        if (!initWinsock()) {
            cerr << "Failed to initialize Winsock" << endl;
        }
    #endif
}

AuthServer::~AuthServer() {
    stop();
    #ifdef _WIN32
        cleanupWinsock();
    #endif
}

#ifdef _WIN32
bool AuthServer::initWinsock() {
    WSADATA wsaData;
    int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (result != 0) {
        cerr << "WSAStartup failed: " << result << endl;
        return false;
    }
    return true;
}

void AuthServer::cleanupWinsock() {
    WSACleanup();
}
#endif

string AuthServer::urlDecode(const string& str) {
    string result;
    char ch;
    int i, ii;
    
    for (i = 0; i < str.length(); i++) {
        if (str[i] != '%') {
            if (str[i] == '+')
                result += ' ';
            else
                result += str[i];
        } else {
            sscanf(str.substr(i + 1, 2).c_str(), "%x", &ii);
            ch = static_cast<char>(ii);
            result += ch;
            i = i + 2;
        }
    }
    return result;
}

map<string, string> AuthServer::parseQueryParams(const string& query) {
    map<string, string> params;
    stringstream ss(query);
    string pair;
    
    while (getline(ss, pair, '&')) {
        size_t pos = pair.find('=');
        if (pos != string::npos) {
            string key = urlDecode(pair.substr(0, pos));
            string value = urlDecode(pair.substr(pos + 1));
            params[key] = value;
        }
    }
    
    return params;
}

void AuthServer::handleClient(int client_socket) {
    char buffer[4096] = {0};
    int bytes_received = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
    
    if (bytes_received <= 0) {
        close(client_socket);
        return;
    }
    
    buffer[bytes_received] = '\0';
    string request(buffer);
    
    // Parse request line
    istringstream iss(request);
    string method, path, http_version;
    iss >> method >> path >> http_version;
    
    // Parse query parameters
    size_t query_pos = path.find('?');
    string actual_path = path;
    string query_string = "";
    
    if (query_pos != string::npos) {
        actual_path = path.substr(0, query_pos);
        query_string = path.substr(query_pos + 1);
    }
    
    auto params = parseQueryParams(query_string);
    
    // Find and call route handler
    string response;
    if (routes.find(actual_path) != routes.end()) {
        response = routes[actual_path](params);
    } else {
        response = sendErrorResponse("Not Found", 404);
    }
    
    send(client_socket, response.c_str(), response.length(), 0);
    close(client_socket);
}

void AuthServer::addRoute(const string& path, 
                         function<string(const map<string, string>&)> handler) {
    routes[path] = handler;
}

string AuthServer::sendJsonResponse(const string& json_data, int status_code,
                                   const string& content_type) {
    stringstream response;
    response << "HTTP/1.1 " << status_code << " OK\r\n";
    response << "Content-Type: " << content_type << "\r\n";
    response << "Content-Length: " << json_data.length() << "\r\n";
    response << "Access-Control-Allow-Origin: *\r\n";
    response << "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n";
    response << "Access-Control-Allow-Headers: Content-Type, Authorization\r\n";
    response << "Connection: close\r\n";
    response << "\r\n";
    response << json_data;
    
    return response.str();
}

string AuthServer::sendErrorResponse(const string& message, int status_code) {
    string json = "{\"error\":\"" + message + "\",\"status\":" + to_string(status_code) + "}";
    return sendJsonResponse(json, status_code);
}

string AuthServer::sendRedirect(const string& url) {
    stringstream response;
    response << "HTTP/1.1 302 Found\r\n";
    response << "Location: " << url << "\r\n";
    response << "Connection: close\r\n";
    response << "\r\n";
    
    return response.str();
}

void AuthServer::start() {
    #ifdef _WIN32
        server_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    #else
        server_socket = socket(AF_INET, SOCK_STREAM, 0);
    #endif
    
    if (server_socket == INVALID_SOCKET) {
        cerr << "Failed to create socket: " << SOCKET_ERROR_CODE << endl;
        return;
    }
    
    // Allow socket reuse
    int opt = 1;
    if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, 
                   (char*)&opt, sizeof(opt)) < 0) {
        cerr << "Failed to set socket options" << endl;
    }
    
    sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(server_port);
    
    if (bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        cerr << "Failed to bind socket on port " << server_port 
             << ": " << SOCKET_ERROR_CODE << endl;
        return;
    }
    
    if (listen(server_socket, 10) < 0) {
        cerr << "Failed to listen on socket: " << SOCKET_ERROR_CODE << endl;
        return;
    }
    
    running = true;
    cout << "Auth server started on http://" << server_host << ":" << server_port << endl;
    
    server_thread = thread([this]() {
        while (running) {
            sockaddr_in client_addr;
            socklen_t client_len = sizeof(client_addr);
            
            int client_socket = accept(server_socket, 
                                      (struct sockaddr*)&client_addr, 
                                      &client_len);
            
            if (client_socket < 0) {
                if (running) {
                    cerr << "Failed to accept connection: " << SOCKET_ERROR_CODE << endl;
                }
                continue;
            }
            
            thread client_thread(&AuthServer::handleClient, this, client_socket);
            client_thread.detach();
        }
    });
}

void AuthServer::stop() {
    running = false;
    
    if (server_socket != -1) {
        #ifdef _WIN32
            closesocket(server_socket);
        #else
            close(server_socket);
        #endif
        server_socket = -1;
    }
    
    if (server_thread.joinable()) {
        server_thread.join();
    }
    
    cout << "Auth server stopped" << endl;
}