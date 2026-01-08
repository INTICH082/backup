@echo off
chcp 65001 >nul
echo ========================================
echo 🌐 AUTH SERVER WITH ZEROTIER (PRIVATE NETWORK)
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

REM Проверяем ZeroTier установку
echo 🔍 Checking ZeroTier installation...
call download_zerotier.ps1
echo.

REM Проверяем наличие network ID
set NETWORK_ID=
if exist "zerotier_network_id.txt" (
    set /p NETWORK_ID=<zerotier_network_id.txt
    echo 🔑 Found network ID: %NETWORK_ID%
) else (
    echo ⚠️ No zerotier_network_id.txt file found
    echo 📋 Create it with your ZeroTier Network ID from: https://my.zerotier.com
    set /p NETWORK_ID="Enter ZeroTier Network ID: "
    if not "%NETWORK_ID%"=="" (
        echo %NETWORK_ID% > zerotier_network_id.txt
        echo ✅ Saved to zerotier_network_id.txt
    )
)

echo.
echo 🚀 Starting auth server with ZeroTier...
echo 📡 Local: http://localhost:8081
if not "%NETWORK_ID%"=="" (
    echo 🌐 ZeroTier Network: %NETWORK_ID%
    echo.
    echo 📋 Team setup instructions:
    echo   1. Install ZeroTier from https://www.zerotier.com/download/
    echo   2. Join network: zerotier-cli join %NETWORK_ID%
    echo   3. Authorize devices at https://my.zerotier.com
    echo   4. Access via ZeroTier private IP
)
echo.
echo 🛑 Press Ctrl+C to stop server
echo.

cd build
if "%NETWORK_ID%"=="" (
    auth_module.exe --api --zerotier
) else (
    auth_module.exe --api --zerotier --zerotier-network %NETWORK_ID%
)

pause