@echo off
chcp 65001 >nul
echo ===============================
echo 🔐 Compiling Auth Module
echo ===============================
echo.

REM Clean
if exist "build\" rmdir /s /q build
mkdir build

echo 📦 Compiling all files...

REM Compile each file separately
g++ -std=c++17 -DWIN32_LEAN_AND_MEAN -DNOMINMAX -Ubyte -c -Iinclude -o build/main.obj src/main.cpp
g++ -std=c++17 -DWIN32_LEAN_AND_MEAN -DNOMINMAX -Ubyte -c -Iinclude -o build/Config.obj src/Config.cpp
g++ -std=c++17 -DWIN32_LEAN_AND_MEAN -DNOMINMAX -Ubyte -c -Iinclude -o build/GitHubOAuth.obj src/GitHubOAuth.cpp
g++ -std=c++17 -DWIN32_LEAN_AND_MEAN -DNOMINMAX -Ubyte -c -Iinclude -o build/JWT.obj src/JWT.cpp
g++ -std=c++17 -DWIN32_LEAN_AND_MEAN -DNOMINMAX -Ubyte -c -Iinclude -o build/SimpleDB.obj src/SimpleDB.cpp

echo.
echo 🔗 Linking...

REM Вариант 1: Используем .dll.a файлы (рекомендуется)
g++ -o build/auth_module.exe ^
    build/main.obj ^
    build/Config.obj ^
    build/GitHubOAuth.obj ^
    build/JWT.obj ^
    build/SimpleDB.obj ^
    -LC:\msys64\ucrt64\lib ^
    -lcurl.dll ^
    -lssl.dll ^
    -lcrypto.dll ^
    -lws2_32 -lz -lwldap32

if %errorlevel% equ 0 (
    echo.
    echo ✅ SUCCESS! Build complete.
    echo 📍 Run: build\auth_module.exe --api
    pause
    exit /b 0
)

echo.
echo ❌ Первый вариант не сработал, пробуем второй...

REM Вариант 2: Используем прямые пути к .dll.a файлам
g++ -o build/auth_module.exe ^
    build/main.obj ^
    build/Config.obj ^
    build/GitHubOAuth.obj ^
    build/JWT.obj ^
    build/SimpleDB.obj ^
    "C:\msys64\ucrt64\lib\libcurl.dll.a" ^
    "C:\msys64\ucrt64\lib\libssl.dll.a" ^
    "C:\msys64\ucrt64\lib\libcrypto.dll.a" ^
    -lws2_32 -lz -lwldap32

if %errorlevel% equ 0 (
    echo.
    echo ✅ SUCCESS! Build complete.
    echo 📍 Run: build\auth_module.exe --api
    echo.
    echo ⚠️  Не забудьте скопировать DLL файлы:
    echo     copy C:\msys64\ucrt64\bin\libcurl-4.dll build\
    echo     copy C:\msys64\ucrt64\bin\libssl-3-x64.dll build\
    echo     copy C:\msys64\ucrt64\bin\libcrypto-3-x64.dll build\
    pause
    exit /b 0
)

echo.
echo ❌ Второй вариант не сработал, пробуем третий...

REM Вариант 3: Статическая линковка с явным указанием
g++ -o build/auth_module.exe ^
    build/main.obj ^
    build/Config.obj ^
    build/GitHubOAuth.obj ^
    build/JWT.obj ^
    build/SimpleDB.obj ^
    -static ^
    -LC:\msys64\ucrt64\lib ^
    -lcurl -lssl -lcrypto -lws2_32 -lz -lwldap32 -lcrypt32

if %errorlevel% equ 0 (
    echo.
    echo ✅ SUCCESS! Static build complete.
    echo 📍 Run: build\auth_module.exe --api
    pause
    exit /b 0
)

echo.
echo ❌ Все варианты не сработали!
echo.
echo 🔧 Проблема: линкер ожидает __imp_ префиксы для DLL.
echo.
echo Попробуйте в MSYS2 терминале:
echo g++ -o test.exe test.cpp -lcurl -lssl -lcrypto -lws2_32
pause