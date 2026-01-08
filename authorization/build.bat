@echo off
chcp 65001 >nul
echo ===============================
echo 🚀 Starting Auth Server
echo ===============================
echo.

REM Проверяем, скомпилирована ли программа
if not exist "build\auth_module.exe" (
    echo ❌ ERROR: Program not compiled!
    echo.
    echo 📋 First run compile.bat to build the program
    pause
    exit /b 1
)

echo 📦 Copying required DLL files...
echo.

REM Копируем DLL файлы
copy "C:\msys64\ucrt64\bin\libcurl-4.dll" build\ >nul 2>&1
if %errorlevel% equ 0 (echo ✅ libcurl-4.dll) else (echo ⚠️ libcurl-4.dll not found)

copy "C:\msys64\ucrt64\bin\libssl-3-x64.dll" build\ >nul 2>&1
if %errorlevel% equ 0 (echo ✅ libssl-3-x64.dll) else (echo ⚠️ libssl-3-x64.dll not found)

copy "C:\msys64\ucrt64\bin\libcrypto-3-x64.dll" build\ >nul 2>&1
if %errorlevel% equ 0 (echo ✅ libcrypto-3-x64.dll) else (echo ⚠️ libcrypto-3-x64.dll not found)

copy "C:\msys64\ucrt64\bin\zlib1.dll" build\ >nul 2>&1
if %errorlevel% equ 0 (echo ✅ zlib1.dll) else (echo ⚠️ zlib1.dll not found)

echo.
echo ===============================
echo 🔐 Starting Auth Server on port 8081
echo ===============================
echo.
echo 📡 API endpoints will be available at:
echo    http://localhost:8081/health
echo    http://localhost:8081/api/auth/login
echo    http://localhost:8081/api/users/me
echo.
echo 🌐 ZeroTier options:
echo   1. Local only: No additional steps
echo   2. Private network: Use run_with_zerotier.bat
echo.
echo Press Ctrl+C to stop the server
echo.

REM Запускаем программу
cd build
auth_module.exe --api