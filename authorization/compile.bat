@echo off
echo ========================================
echo 🔐 Compiling Student Auth Module v2.0
echo ========================================
echo.

REM Download Crow (all-in-one version)
if not exist "include\crow.h" (
    echo 📥 Downloading Crow HTTP library (all-in-one)...
    powershell -Command "Invoke-WebRequest -Uri 'https://raw.githubusercontent.com/ipkn/crow/master/include/crow.h' -OutFile 'include\crow.h'"
)

echo 🔨 Compiling...
g++ -std=c++17 ^
    -Iinclude ^
    -IC:\vcpkg\installed\x64-windows\include ^
    -LC:\vcpkg\installed\x64-windows\lib ^
    -o auth_module.exe ^
    src/*.cpp ^
    -lcurl -lssl -lcrypto -lbcrypt -lws2_32 -static -lpthread

if %errorlevel% equ 0 (
    echo ✅ Compilation successful!
    echo.
    echo Usage:
    echo   auth_module.exe               - Interactive mode
    echo   auth_module.exe --api        - Start API server on port 8081
    echo   auth_module.exe --api --port 3000  - API server on custom port
) else (
    echo ❌ Compilation failed!
    exit /b 1
)