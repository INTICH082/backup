# download_zerotier.ps1
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "📥 DOWNLOADING ZEROTIER FOR AUTH MODULE" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

$zerotierDir = ".\zerotier"
if (-not (Test-Path $zerotierDir)) {
    New-Item -ItemType Directory -Path $zerotierDir | Out-Null
    Write-Host "✅ Created folder: $zerotierDir" -ForegroundColor Green
}

Set-Location $zerotierDir

Write-Host "🌐 Downloading ZeroTier One for Windows..." -ForegroundColor Yellow

try {
    # Скачиваем ZeroTier One для Windows
    $downloadUrl = "https://download.zerotier.com/RELEASES/1.12.2/dist/ZeroTierOne.msi"
    $msiFile = ".\ZeroTierOne.msi"
    
    Write-Host "📥 Downloading ZeroTier One MSI..." -ForegroundColor Yellow
    Invoke-WebRequest -Uri $downloadUrl -OutFile $msiFile -UserAgent "Mozilla/5.0"
    
    if (Test-Path $msiFile) {
        Write-Host "✅ Download successful: $(Get-Item $msiFile | Select-Object -ExpandProperty Length) bytes" -ForegroundColor Green
        
        Write-Host ""
        Write-Host "📦 Installing ZeroTier One..." -ForegroundColor Yellow
        Write-Host "💡 Please accept UAC prompt if it appears" -ForegroundColor Yellow
        
        # Запускаем установку
        Start-Process msiexec.exe -ArgumentList "/i `"$msiFile`" /quiet /norestart" -Wait
        
        # Ждем установки сервиса
        Start-Sleep -Seconds 5
        
        # Проверяем установку
        $service = Get-Service -Name "ZeroTierOneService" -ErrorAction SilentlyContinue
        if ($service) {
            Write-Host "✅ ZeroTier One service installed successfully" -ForegroundColor Green
            
            # Запускаем сервис если не запущен
            if ($service.Status -ne 'Running') {
                Start-Service -Name "ZeroTierOneService"
                Write-Host "✅ ZeroTier One service started" -ForegroundColor Green
            }
            
            # Получаем ID ноды
            Start-Sleep -Seconds 3
            $nodeId = & "C:\Program Files (x86)\ZeroTier\One\zerotier-cli.bat" info 2>&1 | Select-String "address" | ForEach-Object { $_.ToString().Split(' ')[2] }
            
            if ($nodeId) {
                Write-Host ""
                Write-Host "🎉 ZEROTIER SUCCESSFULLY INSTALLED!" -ForegroundColor Green
                Write-Host "🔧 Node ID: $nodeId" -ForegroundColor Green
                
                # Сохраняем ID ноды в файл
                $nodeId | Out-File "..\zerotier_node_id.txt" -Encoding UTF8
                Write-Host "📁 Node ID saved to: zerotier_node_id.txt" -ForegroundColor Green
                
                Write-Host ""
                Write-Host "📋 NEXT STEPS:" -ForegroundColor Yellow
                Write-Host "1. Create network at https://my.zerotier.com" -ForegroundColor Yellow
                Write-Host "2. Get Network ID (16-digit hex)" -ForegroundColor Yellow
                Write-Host "3. Save it to 'zerotier_network_id.txt' file" -ForegroundColor Yellow
                Write-Host "4. Get API Token from https://my.zerotier.com" -ForegroundColor Yellow
                Write-Host "5. Save API token to 'zerotier_token.txt'" -ForegroundColor Yellow
                Write-Host "6. Run: run_with_zerotier.bat" -ForegroundColor Yellow
                Write-Host ""
                Write-Host "💡 Or join network manually:" -ForegroundColor Cyan
                Write-Host "   & 'C:\Program Files (x86)\ZeroTier\One\zerotier-cli.bat' join NETWORK_ID" -ForegroundColor Cyan
            } else {
                Write-Host "⚠️ Could not get node ID, check ZeroTier installation" -ForegroundColor Yellow
            }
        } else {
            Write-Host "❌ ZeroTier service not found after installation!" -ForegroundColor Red
        }
        
        # Удаляем MSI файл
        Remove-Item $msiFile -Force
    } else {
        Write-Host "❌ Download failed!" -ForegroundColor Red
    }
} catch {
    Write-Host "❌ ERROR: $_" -ForegroundColor Red
    Write-Host "📋 Manual installation:" -ForegroundColor Yellow
    Write-Host "1. Open https://www.zerotier.com/download" -ForegroundColor Yellow
    Write-Host "2. Download Windows installer" -ForegroundColor Yellow
    Write-Host "3. Install ZeroTier One" -ForegroundColor Yellow
    Write-Host "4. Register at https://my.zerotier.com" -ForegroundColor Yellow
}

Set-Location ..
Write-Host ""
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "Press any key to continue..." -ForegroundColor Gray
$null = $Host.UI.RawUI.ReadKey('NoEcho,IncludeKeyDown')