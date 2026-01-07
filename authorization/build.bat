@echo off
chcp 65001 >nul

echo Копирование DLL файлов в build...
copy "C:\msys64\ucrt64\bin\libcurl-4.dll" build\ 2>nul
copy "C:\msys64\ucrt64\bin\libssl-3-x64.dll" build\ 2>nul
copy "C:\msys64\ucrt64\bin\libcrypto-3-x64.dll" build\ 2>nul
copy "C:\msys64\ucrt64\bin\zlib1.dll" build\ 2>nul

echo Готово! Запускаем программу...
cd build
auth_module.exe --api