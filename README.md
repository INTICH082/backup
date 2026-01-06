# Модуль авторизации

## Сборка и запуск

### Требования:
- MinGW (g++)
- libcurl
- OpenSSL

### Сборка:
```bash
# Windows (PowerShell)
.\build.ps1

# Linux/Mac
g++ -std=c++17 -I. -o auth_module *.cpp -lcurl -lssl -lcrypto