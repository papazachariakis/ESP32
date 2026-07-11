param([string]$Port = "auto")

$cli = "$env:LOCALAPPDATA\arduino-cli\arduino-cli.exe"
$sketch = Join-Path $PSScriptRoot "ESP32_Control"
$fqbn = "esp32:esp32:esp32:PartitionScheme=huge_app,FlashSize=4M,UploadSpeed=115200"

Write-Host "=== ESP32 Upload (USB) ===" -ForegroundColor Cyan

if ($Port -eq "auto") {
  $ports = & $cli board list | Select-String "serial|usb|cp210|ch340" -CaseSensitive:$false
  if (-not $ports) {
    Write-Host ""
    Write-Host "ERROR: ESP32 not found." -ForegroundColor Red
    Write-Host "1. Connect board with USB-C data cable"
    Write-Host "2. Install CP2102 driver: https://www.silabs.com/developers/usb-to-uart-bridge-vcp-drivers"
    Write-Host "3. Run: .\upload.ps1 -Port COM5"
    exit 1
  }
}

Write-Host "Compiling..."
& $cli compile --fqbn $fqbn $sketch
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host "Uploading to $Port..."
& $cli upload -p $Port --fqbn $fqbn $sketch
if ($LASTEXITCODE -ne 0) {
    Write-Host ""
    Write-Host "Upload failed. Hold BOOT button and run again." -ForegroundColor Yellow
    exit $LASTEXITCODE
}

Write-Host ""
Write-Host "SUCCESS! Open http://esp32.local in your browser." -ForegroundColor Green
Write-Host "First setup: connect to WiFi 'ESP32-Setup' if portal appears."
