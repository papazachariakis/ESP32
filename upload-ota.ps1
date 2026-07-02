param([string]$OtaHost = "esp32.local")

$cli = "$env:LOCALAPPDATA\arduino-cli\arduino-cli.exe"
$sketch = Join-Path $PSScriptRoot "ESP32_Control"
$fqbn = "esp32:esp32:esp32:PartitionScheme=min_spiffs,FlashSize=4M"

Write-Host "=== ESP32 OTA Upload (WiFi) ===" -ForegroundColor Cyan
Write-Host "Target: $OtaHost (password: esp32ota)"
Write-Host ""

Write-Host "Compiling..."
& $cli compile --fqbn $fqbn $sketch
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host "Uploading over WiFi..."
& $cli upload -p $OtaHost --protocol network --upload-field "password=esp32ota" --fqbn $fqbn $sketch
if ($LASTEXITCODE -ne 0) {
    Write-Host ""
    Write-Host "OTA failed. Check:" -ForegroundColor Yellow
    Write-Host "  1. ESP32 is on and connected to home WiFi"
    Write-Host "  2. You are on the same WiFi network"
    Write-Host "  3. OTA firmware was flashed at least once via USB"
    Write-Host "  4. Try IP instead: upload-ota.ps1 -Host 192.168.99.63"
    exit $LASTEXITCODE
}

Write-Host ""
Write-Host "SUCCESS! OTA update complete." -ForegroundColor Green
