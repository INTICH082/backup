@echo off
echo Compiling Student Auth Module...

REM Создаем папку build если её нет
if not exist "build" mkdir build

echo Cleaning previous build...
del /Q build\*.o 2>nul
del /Q build\*.exe 2>nul

echo Compiling files...

REM Компилируем БЕЗ флагов для byte (они уже в precompiled.h)
g++ -I./include -c src/Config.cpp -o build/Config.o
g++ -I./include -c src/GitHubOAuth.cpp -o build/GitHubOAuth.o -lcurl
g++ -I./include -c src/JWT.cpp -o build/JWT.o -lcrypto
g++ -I./include -c src/SimpleDB.cpp -o build/SimpleDB.o
g++ -I./include -c src/main.cpp -o build/main.o

echo Linking...
g++ build/Config.o build/GitHubOAuth.o build/JWT.o build/SimpleDB.o build/main.o -o build/auth_module.exe -lcurl -lcrypto

if %errorlevel% equ 0 (
    echo ✅ Build successful!
    echo 📁 Output: build/auth_module.exe
) else (
    echo ❌ Build failed!
    pause
)