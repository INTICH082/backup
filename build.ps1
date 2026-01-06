# Сборка модуля авторизации
Write-Host "Сборка модуля авторизации..." -ForegroundColor Green

# Компиляция с нужными флагами
$sources = @(
    "auth.cpp",
    "server.cpp", 
    "database.cpp",
    "main.cpp"
)

# Флаги для MinGW
$cflags = "-I. -I./include -std=c++17 -O2 -Wall"

# Ссылки на библиотеки (для MinGW)
$libs = "-lcurl -lssl -lcrypto"

# Компиляция каждого файла
foreach ($src in $sources) {
    $obj = [System.IO.Path]::ChangeExtension($src, ".o")
    Write-Host "Компиляция $src -> $obj"
    g++ $cflags -c $src -o $obj
    
    if ($LASTEXITCODE -ne 0) {
        Write-Host "❌ Ошибка компиляции $src" -ForegroundColor Red
        exit 1
    }
}

# Сборка исполняемого файла
Write-Host "Сборка исполняемого файла..." -ForegroundColor Green
g++ *.o -o auth_module $libs -lws2_32

if ($LASTEXITCODE -ne 0) {
    Write-Host "❌ Ошибка сборки" -ForegroundColor Red
    exit 1
}

# Очистка промежуточных файлов
Remove-Item *.o -ErrorAction SilentlyContinue

Write-Host "✅ Сборка завершена!" -ForegroundColor Green
Write-Host "Запуск: .\auth_module" -ForegroundColor Yellow