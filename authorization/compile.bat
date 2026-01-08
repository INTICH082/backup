@echo off
chcp 65001 >nul
echo ========================================
echo 🔧 COMPILING AUTH MODULE WITH ZEROTIER
echo ========================================
echo.

REM Проверяем MinGW
where g++ >nul 2>&1
if %errorlevel% neq 0 (
    echo ❌ MinGW g++ not found in PATH!
    echo 📋 Add MinGW\bin to your PATH or run from MinGW shell
    pause
    exit /b 1
)

echo ✅ MinGW g++ found: 
g++ --version | findstr "g++"
echo.

REM Создаем папку build если нет
if not exist build mkdir build

echo 📦 Cleaning old object files...
del /q build\*.o 2>nul
del /q build\auth_module.exe 2>nul

echo 📄 Compiling source files...
echo.

REM ВАЖНО: Добавляем флаги для Windows Unicode
set COMPILE_FLAGS=-std=c++17 -D_CRT_SECURE_NO_WARNINGS -DWIN32_LEAN_AND_MEAN -DUNICODE -D_UNICODE -D_WIN32_WINNT=0x0600

REM Компилируем все файлы
echo 1. main.cpp...
g++ -c src/main.cpp -o build/main.o -Iinclude %COMPILE_FLAGS%
if %errorlevel% neq 0 goto :error

echo 2. Config.cpp...
g++ -c src/Config.cpp -o build/Config.o -Iinclude %COMPILE_FLAGS%
if %errorlevel% neq 0 goto :error

echo 3. GitHubOAuth.cpp...
g++ -c src/GitHubOAuth.cpp -o build/GitHubOAuth.o -Iinclude %COMPILE_FLAGS%
if %errorlevel% neq 0 goto :error

echo 4. JWT.cpp...
g++ -c src/JWT.cpp -o build/JWT.o -Iinclude %COMPILE_FLAGS%
if %errorlevel% neq 0 goto :error

echo 5. SimpleDB.cpp...
g++ -c src/SimpleDB.cpp -o build/SimpleDB.o -Iinclude %COMPILE_FLAGS%
if %errorlevel% neq 0 goto :error

echo 6. TaskDB.cpp...
g++ -c src/TaskDB.cpp -o build/TaskDB.o -Iinclude %COMPILE_FLAGS%
if %errorlevel% neq 0 goto :error

echo 7. ZeroTierManager.cpp...
g++ -c src/ZeroTierManager.cpp -o build/ZeroTierManager.o -Iinclude %COMPILE_FLAGS%
if %errorlevel% neq 0 goto :error

echo 8. AutoSessionManager.cpp...
g++ -c src/AutoSessionManager.cpp -o build/AutoSessionManager.o -Iinclude -std=c++17
if %errorlevel% neq 0 goto :error

echo.
echo ✅ All files compiled successfully!
echo.

echo 🔗 Linking executable...
g++ build/main.o build/Config.o build/GitHubOAuth.o build/JWT.o build/SimpleDB.o build/TaskDB.o build/ZeroTierManager.o build/AutoSessionManager.o -o build/auth_module.exe -lcurl -lssl -lcrypto -lws2_32 -lwininet -lole32 -loleaut32 -luuid -lurlmon -std=c++17

if %errorlevel% equ 0 (
    echo.
    echo 🎉 COMPILATION SUCCESSFUL!
    echo ===========================
    echo 📁 Executable: build\auth_module.exe
    for %%F in (build\auth_module.exe) do echo 📏 Size: %%~zF bytes
    echo.
    echo 🌐 ZEROTIER LAUNCH OPTIONS:
    echo.
    echo   1. LOCAL ONLY (for testing):
    echo      build\auth_module.exe --api
    echo.
    echo   2. WITH ZEROTIER (for private team access):
    echo      build\auth_module.exe --api --zerotier
    echo      build\auth_module.exe --api --zerotier --zerotier-network NETWORK_ID
    echo.
    echo   3. WITH CUSTOM PORT:
    echo      build\auth_module.exe --api --port 3000
    echo      build\auth_module.exe --api --port 3000 --zerotier
    echo.
    echo 📋 First time setup:
    echo   - Run: download_zerotier.ps1
    echo   - Create network at https://my.zerotier.com
    echo   - Save network ID to zerotier_network_id.txt
    echo   - Join network manually or use command above
    echo.
) else (
    :error
    echo.
    echo ❌ COMPILATION FAILED!
    echo 📋 Check the errors above
    echo 💡 Make sure you have libcurl, openssl libraries
    echo 💡 Windows: urlmon.lib is needed for URLDownloadToFileW
)

echo.
pause