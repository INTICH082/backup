@echo off
chcp 65001 >nul
echo ===============================
echo 🔐 Compiling Auth Module
echo ===============================
echo.

REM Очистка
if exist "build\" rmdir /s /q build
mkdir build

echo 📦 Compiling all files...

REM Компиляция с исправлением конфликта byte
g++ -std=c++17 -DWIN32_LEAN_AND_MEAN -DNOMINMAX -Ubyte -c -Iinclude -o build/main.obj src/main.cpp
g++ -std=c++17 -DWIN32_LEAN_AND_MEAN -DNOMINMAX -Ubyte -c -Iinclude -o build/Config.obj src/Config.cpp
g++ -std=c++17 -DWIN32_LEAN_AND_MEAN -DNOMINMAX -Ubyte -c -Iinclude -o build/GitHubOAuth.obj src/GitHubOAuth.cpp
g++ -std=c++17 -DWIN32_LEAN_AND_MEAN -DNOMINMAX -Ubyte -c -Iinclude -o build/JWT.obj src/JWT.cpp
g++ -std=c++17 -DWIN32_LEAN_AND_MEAN -DNOMINMAX -Ubyte -c -Iinclude -o build/SimpleDB.obj src/SimpleDB.cpp

echo.
echo 🔗 Linking...

REM Линковка с DLL библиотеками (рекомендуемый способ)
g++ -o build/auth_module.exe ^
    build/main.obj ^
    build/Config.obj ^
    build/GitHubOAuth.obj ^
    build/JWT.obj ^
    build/SimpleDB.obj ^
    -lcurl -lssl -lcrypto -lws2_32 -lz -lwldap32

if %errorlevel% equ 0 (
    echo.
    echo ✅ COMPILATION SUCCESSFUL!
    echo.
    echo 📋 Next step: Run build.bat to copy DLLs and start the server
    echo.
    pause
) else (
    echo.
    echo ❌ COMPILATION FAILED!
    echo Error code: %errorlevel%
    pause
    exit /b 1
)