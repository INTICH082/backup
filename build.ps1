Write-Host "=== Сборка модуля авторизации (SQLite версия) ==="

$gcc = "C:\msys64\ucrt64\bin\g++.exe"
$project = "C:\Users\KSK-SHOP\projects\group_project\group_project"

mkdir -Force build
cd build

# Компиляция с SQLite (предполагаем, что SQLite установлен в MSYS2)
& $gcc -c "$project\authorization\database.cpp" -I"$project\authorization" -std=c++11
& $gcc -c "$project\authorization\auth.cpp" -I"$project\authorization" -std=c++11
& $gcc -c "$project\authorization\server.cpp" -I"$project\authorization" -std=c++11
& $gcc -c "$project\authorization\main.cpp" -I"$project\authorization" -std=c++11

# Линковка с SQLite и CURL
& $gcc database.o auth.o server.o main.o -o auth_module.exe -lcurl -lsqlite3 -lws2_32

Write-Host "`n✅ Сборка завершена!"
Write-Host "Запуск: .\auth_module.exe"
Write-Host "БД будет создана в файле auth.db"

cd ..