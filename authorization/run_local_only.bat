@echo off
chcp 65001 >nul
echo ========================================
echo 🖥️ LOCAL SERVER ONLY
echo ========================================
echo.

if not exist "build\auth_module.exe" (
    echo ❌ auth_module.exe not found!
    echo 📋 Run compile.bat first
    pause
    exit /b 1
)

echo ✅ Starting local server...
echo 📡 Available at: http://localhost:8081
echo 📋 Endpoints:
echo   http://localhost:8081/health
echo   http://localhost:8081/api/auth/login
echo   http://localhost:8081/api/users/register
echo.
echo 📝 Press Ctrl+C to stop
echo ========================================
echo.

cd build
auth_module.exe --api --port 8081
pause