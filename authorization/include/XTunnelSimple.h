#ifndef XTUNNEL_SIMPLE_H
#define XTUNNEL_SIMPLE_H

#include <string>

using namespace std;

class XTunnelSimple {
public:
    // Проверка наличия xTunnel
    static bool isAvailable();
    
    // Запуск туннеля
    static bool startTunnel(int port = 8081, const string& apiKey = "");
    
    // Остановка туннеля
    static void stopTunnel();
    
    // Получение URL туннеля
    static string getTunnelUrl();
    
    // Аутентификация
    static bool authenticate(const string& apiKey);
};

#endif // XTUNNEL_SIMPLE_H