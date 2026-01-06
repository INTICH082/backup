#include "server.h"
#include "auth.h"
#include "config.h"

// Кросс-платформенные заголовки для сетевых сокетов
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#define close closesocket
#define SHUT_RDWR SD_BOTH
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <cerrno>
#include <cstdlib>
#endif

#include <iostream>
#include <sstream>
#include <cctype>      // ДОБАВИТЬ
#include <algorithm>   // ДОБАВИТЬ

using namespace std;

// Определения для кросс-платформенности
#ifdef _WIN32
typedef SOCKET SocketType;
#define INVALID_SOCKET_VAL INVALID_SOCKET
#else
typedef int SocketType;
#define INVALID_SOCKET_VAL (-1)
#endif

// ДОБАВИТЬ ФУНКЦИЮ ДЕКОДИРОВАНИЯ URL
string urlDecode(const string& str) {
    string result;
    char ch;
    int i, ii;
    
    for (i = 0; i < str.length(); i++) {
        if(str[i] == '%') {
            if (i + 2 >= str.length()) break;
            sscanf(str.substr(i + 1, 2).c_str(), "%x", &ii);
            ch = static_cast<char>(ii);
            result += ch;
            i = i + 2;
        } else if(str[i] == '+') {
            result += ' ';
        } else {
            result += str[i];
        }
    }
    return result;
}

void sendResponse(SocketType client, const string& content, bool json = false) {
    string response = "HTTP/1.1 200 OK\r\nContent-Type: " + 
                     string(json ? "application/json" : "text/plain") + 
                     "\r\nConnection: close\r\n\r\n" + content;
    send(client, response.c_str(), response.length(), 0);
}

void sendError(SocketType client, const string& error) {
    string response = "HTTP/1.1 400 Bad Request\r\nContent-Type: application/json\r\n\r\n" 
                     "{\"success\":false,\"error\":\"" + error + "\"}";
    send(client, response.c_str(), response.length(), 0);
}

string readRequest(SocketType client) {
    char buffer[4096] = {0};
    int bytes = recv(client, buffer, sizeof(buffer), 0);
    return bytes > 0 ? string(buffer, bytes) : "";
}

void handleClient(SocketType client) {
    string request = readRequest(client);
    if (request.empty()) {
        close(client);
        return;
    }
    
    istringstream ss(request);
    string method, path;
    ss >> method >> path;
    
    cout << method << " " << path << endl;
    
    // ДОБАВИТЬ health-check
    if (path == "/health") {
        string response = "{\"status\":\"ok\",\"service\":\"auth\",\"timestamp\":" + 
                         to_string(time(nullptr)) + "}";
        sendResponse(client, response, true);
        close(client);
        return;
    }
    
    if (path == "/" || path == "/api") {
        string apiInfo = R"({
    "service": "authorization",
    "version": "1.0",
    "endpoints": {
        "POST /auth/register": "login,password,fullname,email",
        "POST /auth/login": "login,password",
        "POST /auth/telegram": "telegram_id,name",
        "GET /auth/verify?token=...": "verify token",
        "POST /auth/refresh": "refresh_token",
        "GET /auth/oauth?login_token=...": "start OAuth",
        "GET /auth/callback?code=...&state=...": "GitHub callback"
    },
    "health": "GET /health"
})";
        sendResponse(client, apiInfo, true);
        close(client);
        return;
    }
    
    // ДОБАВЛЕНО: декодирование параметров
    if (path == "/auth/register" && method == "POST") {
        size_t body_start = request.find("\r\n\r\n");
        if (body_start == string::npos) {
            sendError(client, "Нет тела запроса");
            close(client);
            return;
        }
        
        string body = request.substr(body_start + 4);
        body = urlDecode(body);  // ДЕКОДИРУЕМ!
        
        istringstream iss(body);
        string pair, login, password, fullname, email;
        
        while (getline(iss, pair, '&')) {
            size_t eq = pair.find('=');
            if (eq != string::npos) {
                string key = pair.substr(0, eq);
                string value = pair.substr(eq + 1);
                
                if (key == "login") login = value;
                else if (key == "password") password = value;
                else if (key == "fullname") fullname = value;
                else if (key == "email") email = value;
            }
        }
        
        string result = Auth::registerUser(login, password, fullname, email);
        sendResponse(client, result, true);
        close(client);
        return;
    }
    
    // ... аналогично для всех POST-эндпоинтов добавить urlDecode(body)
    // Везде где есть парсинг тела запроса
    
    // Также для GET-параметров:
    if (path.find("/auth/verify?") == 0) {
        size_t token_pos = path.find("token=");
        if (token_pos != string::npos) {
            string token = path.substr(token_pos + 6);
            token = urlDecode(token);  // ДЕКОДИРУЕМ!
            string result = Auth::verifyToken(token);
            sendResponse(client, result, true);
        } else {
            sendError(client, "Нет токена");
        }
        close(client);
        return;
    }
    
    // ... аналогично для всех GET-эндпоинтов с параметрами
    
    sendError(client, "Эндпоинт не найден");
    close(client);
}

// Остальная часть server.cpp без изменений...