// XTunnelSimple.cpp

// ========= ВАЖНО: Этот блок должен быть ПЕРВЫМ в файле =========
#ifdef _WIN32
// Сохраняем и очищаем определение byte перед инклюдами
#pragma push_macro("byte")
#pragma push_macro("BYTE")

// Явно убираем byte
#ifdef byte
#undef byte
#endif

#ifdef BYTE
#undef BYTE
#endif

// Определяем _NO_BYTE чтобы Windows заголовки не определяли byte
#define _NO_BYTE
#endif
// ===============================================================

// Теперь безопасно инклюдить заголовки
#include "../include/precompiled.h"
#include "../include/XTunnelSimple.h"

#include <iostream>
#include <fstream>
#include <cstdlib>
#include <string>

#ifdef _WIN32
#include <windows.h>
#include <process.h>
#define popen _popen
#define pclose _pclose
#else
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#endif

using namespace std;

#ifdef _WIN32
// Восстанавливаем оригинальные макросы после инклюдов
#pragma pop_macro("byte")
#pragma pop_macro("BYTE")
#endif

bool XTunnelSimple::isAvailable() {
#ifdef _WIN32
    ifstream file1("xtunnel\\xtunnel.exe");
    if (file1.good()) {
        file1.close();
        return true;
    }
    
    ifstream file2("build\\xtunnel.exe");
    if (file2.good()) {
        file2.close();
        return true;
    }
    
    // Проверяем в PATH
    string command = "where xtunnel >nul 2>&1";
    int result = system(command.c_str());
    return result == 0;
#else
    return false;
#endif
}

bool XTunnelSimple::startTunnel(int port, const string& apiKey) {
#ifdef _WIN32
    string command;
    
    // Сначала аутентифицируем если есть ключ
    if (!apiKey.empty()) {
        string auth_cmd = "xtunnel\\xtunnel.exe auth \"" + apiKey + "\" >nul 2>&1";
        system(auth_cmd.c_str());
    }
    
    // Запускаем туннель в отдельном процессе
    command = "start \"xTunnel\" cmd /c \"xtunnel\\xtunnel.exe " + to_string(port) + "\"";
    
    int result = system(command.c_str());
    if (result != 0) {
        // Попробуем альтернативный способ
        command = "xtunnel\\xtunnel.exe " + to_string(port) + " > xtunnel_log.txt 2>&1 &";
        result = system(command.c_str());
    }
    
    // Даем время на запуск
#ifdef _WIN32
    Sleep(3000);
#else
    sleep(3);
#endif
    
    return result == 0;
#else
    return false;
#endif
}

void XTunnelSimple::stopTunnel() {
#ifdef _WIN32
    system("taskkill /F /IM xtunnel.exe 2>nul >nul");
    system("taskkill /F /FI \"WINDOWTITLE eq xTunnel*\" 2>nul >nul");
    
    // Также убиваем связанные cmd процессы
    system("taskkill /F /FI \"WINDOWTITLE eq cmd*\" /FI \"MEMUSAGE gt 1000\" 2>nul >nul");
#endif
}

string XTunnelSimple::getTunnelUrl() {
#ifdef _WIN32
    FILE* pipe = popen("xtunnel\\xtunnel.exe list 2>&1", "r");
    if (!pipe) {
        return "";
    }
    
    char buffer[256];
    string result = "";
    
    while (fgets(buffer, sizeof(buffer), pipe) != NULL) {
        result += buffer;
    }
    
    pclose(pipe);
    
    // Ищем URL в выводе
    size_t pos = result.find("https://");
    if (pos != string::npos) {
        size_t end = result.find_first_of("\n\r ", pos);
        if (end != string::npos) {
            return result.substr(pos, end - pos);
        }
        return result.substr(pos);
    }
    
    // Проверяем альтернативный формат
    pos = result.find("http://");
    if (pos != string::npos) {
        size_t end = result.find_first_of("\n\r ", pos);
        if (end != string::npos) {
            return result.substr(pos, end - pos);
        }
        return result.substr(pos);
    }
    
    return "";
#else
    return "";
#endif
}

bool XTunnelSimple::authenticate(const string& apiKey) {
#ifdef _WIN32
    if (apiKey.empty()) {
        return false;
    }
    
    string command = "xtunnel\\xtunnel.exe auth \"" + apiKey + "\" >nul 2>&1";
    int result = system(command.c_str());
    
    if (result != 0) {
        // Попробуем без подавления вывода для отладки
        command = "xtunnel\\xtunnel.exe auth \"" + apiKey + "\"";
        result = system(command.c_str());
    }
    
    return result == 0;
#else
    return false;
#endif
}