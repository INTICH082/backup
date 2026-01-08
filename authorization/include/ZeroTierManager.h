#ifndef ZEROTIER_MANAGER_H
#define ZEROTIER_MANAGER_H

#include <string>
#include <vector>

using namespace std;

struct ZeroTierNode {
    string node_id;
    string name;
    vector<string> ip_addresses;
    bool online;
};

struct ZeroTierNetwork {
    string network_id;
    string name;
    vector<string> assigned_addresses;
};

class ZeroTierManager {
private:
    string controller_url;
    string network_id;
    string node_id;
    string api_token;
    
    string executeCommand(const string& command);
    string makeAPIRequest(const string& endpoint, const string& method = "GET", const string& data = "");
    bool installZeroTier();
    ZeroTierNetwork getNetworkInfo(const string& nwid = "");
    
public:
    ZeroTierManager();
    
    // Основные функции
    bool initialize();
    bool isInstalled();
    bool joinNetwork(const string& network_id);
    bool leaveNetwork();
    bool leaveNetwork(const string& nwid);
    
    // Получение информации
    vector<string> getLocalIPs();
    string getNodeID();
    string getStatus();
    string getServerURL(int port);
    string getZeroTierPath();
    
    // Состояние
    bool isConnected();
    
    // Управление сетями и участниками
    bool authorizeMember(const string& node_id, const string& network_id = "");
    bool deauthorizeMember(const string& node_id, const string& network_id = "");
    bool createNetwork(const string& name, const string& description = "");
    vector<ZeroTierNode> getNetworkMembers(const string& network_id = "");
    
    // Утилиты
    static string generateRandomNetworkID();
};

#endif // ZEROTIER_MANAGER_H