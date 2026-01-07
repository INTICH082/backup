@echo off
if not exist auth_module.exe (
    echo Building first...
    call compile.bat
)

if exist auth_module.exe (
    echo.
    echo Starting Auth Module...
    echo.
    auth_module.exe
) else (
    echo Failed to build or find executable
)

pause