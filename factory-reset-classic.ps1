param(
  [string]$Port = "COM5",
  [switch]$SkipErase
)

$ErrorActionPreference = "Stop"
$cli = "$env:LOCALAPPDATA\arduino-cli\arduino-cli.exe"
$root = $PSScriptRoot
$sketch = Join-Path $root "ESP32_Control"
$fqbn = "esp32:esp32:esp32:PartitionScheme=min_spiffs,FlashSize=4M,UploadSpeed=115200"

Write-Host "=== ESP32 Classic FACTORY RESET + FRESH FIRMWARE ===" -ForegroundColor Cyan
Write-Host "Port: $Port"

if (-not $SkipErase) {
  $esptool = Get-ChildItem "$env:LOCALAPPDATA\Arduino15\packages\esp32\tools\esptool_py" -Recurse -Filter "esptool.exe" -ErrorAction SilentlyContinue |
    Sort-Object FullName -Descending | Select-Object -First 1
  if (-not $esptool) {
    Write-Host "ERROR: esptool.exe not found." -ForegroundColor Red
    exit 1
  }
  Write-Host "Erasing flash (full factory wipe)..." -ForegroundColor Yellow
  & $esptool.FullName --chip esp32 -p $Port erase-flash
  if ($LASTEXITCODE -ne 0) {
    Write-Host "Erase failed. Close Serial Monitor, hold BOOT if needed, retry." -ForegroundColor Red
    exit $LASTEXITCODE
  }
  Write-Host "Flash erased OK." -ForegroundColor Green
  Start-Sleep -Seconds 2
}

Write-Host "Compiling + uploading Classic..."
& $cli compile --fqbn $fqbn $sketch
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& $cli upload -p $Port --fqbn $fqbn $sketch
if ($LASTEXITCODE -ne 0) {
  Write-Host "Upload failed. Hold BOOT, tap RESET, release BOOT, run again." -ForegroundColor Yellow
  exit $LASTEXITCODE
}

$binOut = Join-Path $root "docs\firmware.bin"
$built = Get-ChildItem "$env:LOCALAPPDATA\arduino\sketches" -Recurse -Filter "ESP32_Control.ino.bin" |
  Sort-Object LastWriteTime -Descending | Select-Object -First 1
if ($built) {
  Copy-Item $built.FullName $binOut -Force
  Write-Host "Copied $($built.Length) bytes -> docs/firmware.bin"
}

Write-Host ""
Write-Host "FACTORY RESET COMPLETE" -ForegroundColor Green
Write-Host "Connect phone to WiFi 'ESP32-Setup' and set your home SSID (e.g. OTEc70dd0)."
Write-Host "Then open http://esp32.local or check DHCP for the new IP."
