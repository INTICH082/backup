#ifdef _WIN32
// Определяем макросы перед Windows заголовками
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#ifndef NOMINMAX
#define NOMINMAX
#endif

// ВАЖНО: Полностью отключаем std::byte для Windows
#define _HAS_STD_BYTE 0
#define __cpp_lib_byte 0
#endif

// Включаем cstddef ПЕРЕД Windows заголовками, но после отключения std::byte
#include <cstddef>

#ifdef _WIN32
// ВАЖНО: Переименовываем Windows byte перед включением заголовков
#ifdef byte
#undef byte
#define byte windows_byte_renamed
#endif

// Теперь включаем Windows заголовки
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <urlmon.h>
#include <wininet.h>
#include <process.h>
#include <shellapi.h>

// Восстанавливаем byte для Windows API (если нужно)
#ifdef windows_byte_renamed
#undef byte
#define byte unsigned char
#endif

#pragma comment(lib, "wininet.lib")
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "urlmon.lib")

#define popen _popen
#define pclose _pclose
#else
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif

// Теперь включаем остальные заголовки
#include "../include/ZeroTierManager.h"
#include "../include/precompiled.h"

#include <iostream>
#include <fstream>
#include <sstream>
#include <chrono>
#include <thread>
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <vector>
#include <string>

using namespace std;

// Конструктор
ZeroTierManager::ZeroTierManager() : 
    controller_url("http://localhost:9993"),
    network_id(""),
    node_id(""),
    api_token("") {
}

string ZeroTierManager::executeCommand(const string& command) {
#ifdef _WIN32
    FILE* pipe = _popen(command.c_str(), "r");
#else
    FILE* pipe = popen(command.c_str(), "r");
#endif
    if (!pipe) return "";
    
    char buffer[128];
    string result = "";
    while (fgets(buffer, sizeof(buffer), pipe) != NULL) {
        result += buffer;
    }
    
#ifdef _WIN32
    _pclose(pipe);
#else
    pclose(pipe);
#endif
    
    return result;
}

string ZeroTierManager::makeAPIRequest(const string& endpoint, const string& method, const string& data) {
#ifdef _WIN32
    HINTERNET hInternet = InternetOpenA("ZeroTierManager/1.0", 
        INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);
    if (!hInternet) return "";
    
    string url = controller_url + endpoint;
    HINTERNET hConnect = InternetOpenUrlA(hInternet, url.c_str(), 
        ("X-ZT1-Auth: " + api_token).c_str(), -1, 
        INTERNET_FLAG_RELOAD, 0);
    
    if (!hConnect) {
        InternetCloseHandle(hInternet);
        return "";
    }
    
    string response;
    char buffer[4096];
    DWORD bytesRead;
    while (InternetReadFile(hConnect, buffer, sizeof(buffer), &bytesRead) && bytesRead > 0) {
        response.append(buffer, bytesRead);
    }
    
    InternetCloseHandle(hConnect);
    InternetCloseHandle(hInternet);
    return response;
#else
    // Linux/Mac implementation would use libcurl
    // Simplified for now
    string cmd = "curl -s -H \"X-ZT1-Auth: " + api_token + "\" ";
    cmd += "-X " + method + " ";
    if (!data.empty()) cmd += "-d \"" + data + "\" ";
    cmd += controller_url + endpoint;
    
    return executeCommand(cmd);
#endif
}

bool ZeroTierManager::isInstalled() {
#ifdef _WIN32
    // Check if ZeroTier service is running
    SC_HANDLE scm = OpenSCManager(NULL, NULL, SC_MANAGER_CONNECT);
    if (!scm) return false;
    
    SC_HANDLE service = OpenServiceW(scm, L"ZeroTierOneService", SERVICE_QUERY_STATUS);
    if (!service) {
        CloseServiceHandle(scm);
        return false;
    }
    
    SERVICE_STATUS status;
    bool isRunning = QueryServiceStatus(service, &status) && 
                     status.dwCurrentState == SERVICE_RUNNING;
    
    CloseServiceHandle(service);
    CloseServiceHandle(scm);
    return isRunning;
#else
    string result = executeCommand("which zerotier-cli");
    return !result.empty() && result.find("not found") == string::npos;
#endif
}

bool ZeroTierManager::installZeroTier() {
#ifdef _WIN32
    // Download and install ZeroTier
    string downloadUrl = "https://download.zerotier.com/RELEASES/1.12.2/dist/ZeroTierOne.msi";
    string msiFile = "ZeroTierOne.msi";
    
    cout << "[ZeroTier] Downloading installer..." << endl;
    
    // Use URLDownloadToFileW with wstring conversion
    std::wstring wDownloadUrl = std::wstring(downloadUrl.begin(), downloadUrl.end());
    std::wstring wMsiFile = std::wstring(msiFile.begin(), msiFile.end());
    
    HRESULT hr = URLDownloadToFileW(NULL, 
        wDownloadUrl.c_str(),
        wMsiFile.c_str(),
        0, NULL);
    
    if (FAILED(hr)) {
        cerr << "[ZeroTier] Failed to download installer" << endl;
        return false;
    }
    
    // Install using msiexec
    cout << "[ZeroTier] Installing..." << endl;
    string installCmd = "msiexec /i \"" + msiFile + "\" /quiet /norestart";
    int result = system(installCmd.c_str());
    
    // Delete installer
    DeleteFileW(wMsiFile.c_str());
    
    if (result != 0) {
        cerr << "[ZeroTier] Installation failed" << endl;
        return false;
    }
    
    // Wait for service to start
    this_thread::sleep_for(chrono::seconds(5));
    
    // Get node ID
    node_id = getNodeID();
    
    if (node_id.empty()) {
        cerr << "[ZeroTier] Could not get node ID after installation" << endl;
        return false;
    }
    
    cout << "[ZeroTier] Installed successfully. Node ID: " << node_id << endl;
    return true;
#else
    // Linux installation
    cout << "[ZeroTier] Installing on Linux..." << endl;
    system("curl -s https://install.zerotier.com | sudo bash");
    this_thread::sleep_for(chrono::seconds(3));
    return isInstalled();
#endif
}

bool ZeroTierManager::joinNetwork(const string& nwid) {
    if (nwid.empty()) {
        if (network_id.empty()) {
            cerr << "[ZeroTier] No network ID specified" << endl;
            return false;
        }
        return joinNetwork(network_id);
    }
    
    network_id = nwid;
    
    if (!isInstalled()) {
        cerr << "[ZeroTier] Not installed" << endl;
        return false;
    }
    
    cout << "[ZeroTier] Joining network: " << nwid << endl;
    
    // Join network
    string cmd = "zerotier-cli join " + nwid;
    string result = executeCommand(cmd);
    
    // Wait for connection
    for (int i = 0; i < 30; i++) {
        vector<string> ips = getLocalIPs();
        if (!ips.empty()) {
            cout << "[ZeroTier] Successfully joined network. IP: " << ips[0] << endl;
            return true;
        }
        this_thread::sleep_for(chrono::seconds(2));
        cout << "[ZeroTier] Waiting for IP assignment... (" << (i+1) << "/30)" << endl;
    }
    
    cerr << "[ZeroTier] Timeout waiting for IP assignment" << endl;
    return false;
}

bool ZeroTierManager::leaveNetwork(const string& nwid) {
    string target_nwid = nwid.empty() ? network_id : nwid;
    if (target_nwid.empty()) return false;
    
    string cmd = "zerotier-cli leave " + target_nwid;
    string result = executeCommand(cmd);
    
    if (target_nwid == network_id) {
        network_id = "";
    }
    
    return result.find("200 leave OK") != string::npos;
}

ZeroTierNetwork ZeroTierManager::getNetworkInfo(const string& nwid) {
    ZeroTierNetwork network;
    string target_nwid = nwid.empty() ? network_id : nwid;
    
    if (target_nwid.empty()) return network;
    
    // Use local CLI for network info
    string cmd = "zerotier-cli listnetworks";
    string result = executeCommand(cmd);
    
    // Parse result
    istringstream stream(result);
    string line;
    while (getline(stream, line)) {
        if (line.find(target_nwid) != string::npos) {
            istringstream line_stream(line);
            vector<string> parts;
            string part;
            while (line_stream >> part) {
                parts.push_back(part);
            }
            
            if (parts.size() >= 4) {
                network.network_id = parts[0];
                network.name = parts[1];
                if (parts.size() > 4) {
                    for (size_t i = 4; i < parts.size(); i++) {
                        network.assigned_addresses.push_back(parts[i]);
                    }
                }
            }
            break;
        }
    }
    
    return network;
}

vector<string> ZeroTierManager::getLocalIPs() {
    vector<string> ips;
    
    ZeroTierNetwork network = getNetworkInfo();
    if (!network.assigned_addresses.empty()) {
        return network.assigned_addresses;
    }
    
    // Alternative method: check network interfaces
#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        return ips;
    }
    
    char hostname[256];
    if (gethostname(hostname, sizeof(hostname)) == 0) {
        struct addrinfo hints = {}, *addrs;
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_protocol = IPPROTO_TCP;
        
        if (getaddrinfo(hostname, NULL, &hints, &addrs) == 0) {
            for (struct addrinfo* addr = addrs; addr != NULL; addr = addr->ai_next) {
                char ip[INET_ADDRSTRLEN];
                struct sockaddr_in* sa = (struct sockaddr_in*)addr->ai_addr;
                inet_ntop(AF_INET, &sa->sin_addr, ip, INET_ADDRSTRLEN);
                
                // Filter out localhost and common private IPs
                string ip_str(ip);
                if (ip_str.find("169.254.") != 0 && 
                    ip_str != "127.0.0.1" && 
                    ip_str != "0.0.0.0") {
                    ips.push_back(ip_str);
                }
            }
            freeaddrinfo(addrs);
        }
    }
    
    WSACleanup();
#endif
    
    return ips;
}

string ZeroTierManager::getNodeID() {
    if (!node_id.empty()) return node_id;
    
    string result = executeCommand("zerotier-cli info");
    size_t pos = result.find("address");
    if (pos != string::npos) {
        istringstream stream(result.substr(pos));
        string dummy;
        stream >> dummy >> node_id;
    }
    
    return node_id;
}

string ZeroTierManager::getServerURL(int port) {
    vector<string> ips = getLocalIPs();
    if (ips.empty()) return "";
    
    return "http://" + ips[0] + ":" + to_string(port);
}

bool ZeroTierManager::initialize() {
    if (!isInstalled()) {
        cout << "[ZeroTier] ZeroTier not found, installing..." << endl;
        if (!installZeroTier()) {
            return false;
        }
    }
    
    // Get node ID
    node_id = getNodeID();
    if (node_id.empty()) {
        cerr << "[ZeroTier] Failed to get node ID" << endl;
        return false;
    }
    
    cout << "[ZeroTier] Initialized. Node ID: " << node_id << endl;
    return true;
}

bool ZeroTierManager::isConnected() {
    return !getLocalIPs().empty();
}

string ZeroTierManager::getStatus() {
    if (!isInstalled()) return "not_installed";
    if (!isConnected()) return "disconnected";
    return "connected";
}

// Static utility functions
string ZeroTierManager::generateRandomNetworkID() {
    const char hex_chars[] = "0123456789abcdef";
    string network_id;
    
    srand(time(NULL));
    for (int i = 0; i < 16; i++) {
        network_id += hex_chars[rand() % 16];
    }
    
    return network_id;
}

string ZeroTierManager::getZeroTierPath() {
#ifdef _WIN32
    return "C:\\Program Files (x86)\\ZeroTier\\One\\zerotier-cli.bat";
#else
    return "/usr/sbin/zerotier-cli";
#endif
}

// Реализации недостающих методов
vector<ZeroTierNode> ZeroTierManager::getNetworkMembers(const string& network_id) {
    return vector<ZeroTierNode>(); // Заглушка
}

bool ZeroTierManager::authorizeMember(const string& node_id, const string& network_id) {
    return false; // Заглушка
}

bool ZeroTierManager::deauthorizeMember(const string& node_id, const string& network_id) {
    return false; // Заглушка
}

bool ZeroTierManager::createNetwork(const string& name, const string& description) {
    return false; // Заглушка
}