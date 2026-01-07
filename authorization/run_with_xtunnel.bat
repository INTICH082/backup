@echo off
chcp 65001 >nul
echo ========================================
echo 🚀 AUTH SERVER WITH XTUNNEL (PUBLIC ACCESS)
echo ========================================
echo.

REM Проверяем наличие скомпилированной программы
if not exist "build\auth_module.exe" (
    echo ❌ ERROR: auth_module.exe not found!
    echo 📋 First compile with: compile.bat
    pause
    exit /b 1
)

echo ✅ Auth module found
echo.

REM 1. Проверяем и скачиваем xTunnel если нет
if not exist "xtunnel\xtunnel.exe" (
    echo ❌ xTunnel not found!
    echo 📥 Downloading xTunnel...
    
    powershell -Command ^
        "if (!(Test-Path 'xtunnel')) { New-Item -ItemType Directory -Path 'xtunnel' };" ^
        "Invoke-WebRequest -Uri 'https://cdn.xtunnel.ru/releases/windows/latest/xtunnel.zip' -OutFile 'xtunnel.zip';" ^
        "Add-Type -AssemblyName System.IO.Compression.FileSystem;" ^
        "[System.IO.Compression.ZipFile]::ExtractToDirectory('xtunnel.zip', 'xtunnel');" ^
        "Remove-Item 'xtunnel.zip';"
    
    if not exist "xtunnel\xtunnel.exe" (
        echo ❌ Failed to download xTunnel!
        echo 📋 Manual download: https://xtunnel.ru
        pause
        exit /b 1
    )
    echo ✅ xTunnel downloaded
)

echo ✅ xTunnel found at xtunnel\xtunnel.exe
echo.

REM 2. Проверяем ключ
set XT_KEY=
if exist "xtunnel_key.txt" (
    set /p XT_KEY=<xtunnel_key.txt
    echo 🔑 Found API key in xtunnel_key.txt
    echo 🔑 Key: %XT_KEY%
) else (
    echo ⚠️ No xtunnel_key.txt file found
    echo 📋 Create it with your xTunnel API key from: https://xtunnel.ru
    pause
    exit /b 1
)

REM 3. Копируем конфиг для xTunnel
echo 📋 Preparing config...
copy config_xtunnel.json build\config.json >nul
copy "C:\msys64\ucrt64\bin\libcurl-4.dll" build\ >nul 2>&1
copy "C:\msys64\ucrt64\bin\libssl-3-x64.dll" build\ >nul 2>&1
copy "C:\msys64\ucrt64\bin\libcrypto-3-x64.dll" build\ >nul 2>&1

REM 4. Запускаем туннель в ОТДЕЛЬНОМ окне
echo.
echo 🔧 Starting xTunnel tunnel...
echo ⏳ This may take a few seconds...

start "xTunnel Public Tunnel" cmd /c "cd /d "%~dp0xtunnel" && echo Starting xTunnel... && xtunnel.exe auth %XT_KEY% && xtunnel.exe 8081 && pause"

echo ⏳ Waiting for tunnel to start...
timeout /t 3 >nul

REM 5. Получаем URL туннеля
echo 🔍 Getting public URL...
cd xtunnel
for /f "tokens=*" %%i in ('xtunnel.exe list') do (
    echo %%i | findstr "https://" >nul && (
        set TUNNEL_URL=%%i
    )
)
cd ..

if not "%TUNNEL_URL%"=="" (
    echo.
    echo ========================================
    echo 🌐 PUBLIC URL FOR YOUR TEAM:
    echo ========================================
    echo %TUNNEL_URL%
    echo.
    echo 📋 Share this URL with your colleagues!
    echo 📊 API endpoints:
    echo   %TUNNEL_URL%/health
    echo   %TUNNEL_URL%/api/auth/login
    echo   %TUNNEL_URL%/api/users/register
    echo   %TUNNEL_URL%/api/auth/github
    echo ========================================
    echo.
) else (
    echo ⚠️ Could not get tunnel URL automatically
    echo 📋 Check manually: .\xtunnel\xtunnel.exe list
    echo 📋 Or visit: https://xtunnel.ru/dashboard
)

REM 6. Запускаем auth сервер
echo.
echo 🚀 Starting auth server...
echo 📡 Local: http://localhost:8081
if not "%TUNNEL_URL%"=="" (
    echo 🌐 Public: %TUNNEL_URL%
)
echo.
echo 🛑 Press Ctrl+C to stop server AND tunnel
echo.

cd build
auth_module.exe --api --xtunnel --xtunnel-key %XT_KEY%

echo.
echo 🛑 Stopping xTunnel...
taskkill /F /FI "WINDOWTITLE eq xTunnel*" 2>nul
taskkill /F /IM xtunnel.exe 2>nul
echo ✅ Done
pause