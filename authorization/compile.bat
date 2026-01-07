@echo off
chcp 65001 >nul
echo ========================================
echo 🔧 COMPILING WITH MINGW + XTUNNEL
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

REM Упрощенные флаги компиляции
set COMPILE_FLAGS=-std=c++17 -D_CRT_SECURE_NO_WARNINGS

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

echo 6. XTunnelSimple.cpp...
g++ -c src/XTunnelSimple.cpp -o build/XTunnelSimple.o -Iinclude %COMPILE_FLAGS%
if %errorlevel% neq 0 goto :error

echo.
echo ✅ All files compiled successfully!
echo.

echo 🔗 Linking executable...
g++ build/main.o build/Config.o build/GitHubOAuth.o build/JWT.o build/SimpleDB.o build/XTunnelSimple.o -o build/auth_module.exe -lcurl -lssl -lcrypto -lws2_32 -std=c++17

if %errorlevel% equ 0 (
    echo.
    echo 🎉 COMPILATION SUCCESSFUL!
    echo ===========================
    echo 📁 Executable: build\auth_module.exe
    echo 📏 Size: 
    for %%F in (build\auth_module.exe) do echo   %%~zF bytes
    echo.
    echo 🚀 Available launch options:
    echo.
    echo   1. LOCAL ONLY (for testing):
    echo      build\auth_module.exe --api
    echo      OR: build\auth_module.exe
    echo.
    echo   2. WITH XTUNNEL (for team access):
    echo      build\auth_module.exe --api --xtunnel
    echo      build\auth_module.exe --api --xtunnel --xtunnel-key YOUR_KEY
    echo.
    echo   3. WITH CUSTOM PORT:
    echo      build\auth_module.exe --api --port 3000
    echo      build\auth_module.exe --api --port 3000 --xtunnel
    echo.
    echo 📋 First time setup:
    echo   - Run: download_xtunnel.ps1
    echo   - Get API key from https://xtunnel.ru
    echo   - Save key to xtunnel_key.txt
    echo   - Run: run_with_xtunnel.bat
    echo.
) else (
    :error
    echo.
    echo ❌ COMPILATION FAILED!
    echo 📋 Check the errors above
    echo 💡 Make sure you have libcurl, openssl libraries
)

echo.
pause