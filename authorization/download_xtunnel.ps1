# download_xtunnel.ps1
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "📥 DOWNLOADING XTUNNEL FOR AUTH MODULE" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

# Создаем папку для xTunnel
$xtunnelDir = ".\xtunnel"
if (-not (Test-Path $xtunnelDir)) {
    New-Item -ItemType Directory -Path $xtunnelDir | Out-Null
    Write-Host "✅ Created folder: $xtunnelDir" -ForegroundColor Green
}

Set-Location $xtunnelDir

Write-Host "🌐 Downloading xTunnel from https://xtunnel.ru..." -ForegroundColor Yellow

try {
    # URL для скачивания (прямая ссылка на Windows версию)
    $downloadUrl = "https://cdn.xtunnel.ru/releases/windows/latest/xtunnel.zip"
    $zipFile = ".\xtunnel.zip"
    
    # Скачиваем
    Write-Host "📥 Downloading..." -ForegroundColor Yellow
    Invoke-WebRequest -Uri $downloadUrl -OutFile $zipFile -UserAgent "Mozilla/5.0"
    
    if (Test-Path $zipFile) {
        Write-Host "✅ Download successful: $(Get-Item $zipFile | Select-Object -ExpandProperty Length) bytes" -ForegroundColor Green
        
        # Распаковываем
        Write-Host "📦 Extracting files..." -ForegroundColor Yellow
        Add-Type -AssemblyName System.IO.Compression.FileSystem
        [System.IO.Compression.ZipFile]::ExtractToDirectory($zipFile, ".")
        
        # Удаляем архив
        Remove-Item $zipFile
        
        # Копируем в build для удобства
        if (Test-Path "..\build") {
            Copy-Item ".\xtunnel.exe" "..\build\" -Force
            Write-Host "📁 Copied to build/ folder" -ForegroundColor Green
        }
        
        # Проверяем
        if (Test-Path ".\xtunnel.exe") {
            Write-Host ""
            Write-Host "✅ XTUNNEL SUCCESSFULLY INSTALLED!" -ForegroundColor Green
            Write-Host "📁 Location: $(Get-Location)\xtunnel.exe" -ForegroundColor Green
            
            # Показываем версию
            $version = .\xtunnel.exe --version 2>&1
            Write-Host "🔧 Version: $version" -ForegroundColor Green
            
            Write-Host ""
            Write-Host "📋 NEXT STEPS:" -ForegroundColor Yellow
            Write-Host "1. Register at https://xtunnel.ru" -ForegroundColor Yellow
            Write-Host "2. Get your API key from dashboard" -ForegroundColor Yellow
            Write-Host "3. Save it to 'xtunnel_key.txt' file in project root" -ForegroundColor Yellow
            Write-Host "4. Run: run_with_xtunnel.bat" -ForegroundColor Yellow
            Write-Host ""
            Write-Host "💡 Or test manually:" -ForegroundColor Cyan
            Write-Host "   .\xtunnel\xtunnel.exe auth YOUR_API_KEY" -ForegroundColor Cyan
            Write-Host "   .\xtunnel\xtunnel.exe 8081" -ForegroundColor Cyan
        } else {
            Write-Host "❌ xtunnel.exe not found after extraction!" -ForegroundColor Red
        }
    } else {
        Write-Host "❌ Download failed!" -ForegroundColor Red
    }
} catch {
    Write-Host "❌ ERROR: $_" -ForegroundColor Red
    Write-Host "📋 Manual download:" -ForegroundColor Yellow
    Write-Host "1. Open https://xtunnel.ru" -ForegroundColor Yellow
    Write-Host "2. Download for Windows" -ForegroundColor Yellow
    Write-Host "3. Extract to 'xtunnel' folder" -ForegroundColor Yellow
}

Set-Location ..
Write-Host ""
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "Press any key to continue..." -ForegroundColor Gray
$null = $Host.UI.RawUI.ReadKey('NoEcho,IncludeKeyDown')