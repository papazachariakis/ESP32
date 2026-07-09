param(
  [string]$Port = "COM6",
  [switch]$SkipErase
)

$ErrorActionPreference = "Stop"
$cli = "$env:LOCALAPPDATA\arduino-cli\arduino-cli.exe"
$root = $PSScriptRoot
$sketch = Join-Path $root "ESP32_Control"
$fqbn = "esp32:esp32:esp32s3:CDCOnBoot=cdc,USBMode=hwcdc,FlashSize=16M,PartitionScheme=min_spiffs,PSRAM=opi,UploadSpeed=921600"

Write-Host "=== ESP32-S3 FACTORY RESET + FRESH FIRMWARE v3 ===" -ForegroundColor Cyan
Write-Host "Port: $Port"

if (-not $SkipErase) {
  $esptool = Get-ChildItem "$env:LOCALAPPDATA\Arduino15\packages\esp32\tools\esptool_py" -Recurse -Filter "esptool.exe" -ErrorAction SilentlyContinue |
    Sort-Object FullName -Descending | Select-Object -First 1
  if (-not $esptool) {
    Write-Host "ERROR: esptool.exe not found. Install esp32 core in Arduino CLI." -ForegroundColor Red
    exit 1
  }
  Write-Host "Erasing flash (full factory wipe)..." -ForegroundColor Yellow
  & $esptool.FullName --chip esp32s3 -p $Port erase-flash
  if ($LASTEXITCODE -ne 0) {
    Write-Host "Erase failed. Close Serial Monitor, hold BOOT if needed, retry." -ForegroundColor Red
    exit $LASTEXITCODE
  }
  Write-Host "Flash erased OK." -ForegroundColor Green
  Start-Sleep -Seconds 2
}

Write-Host "Compiling Hub v3 for ESP32-S3..."
& $cli compile --fqbn $fqbn $sketch
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host "Uploading fresh firmware to $Port..."
& $cli upload -p $Port --fqbn $fqbn $sketch
if ($LASTEXITCODE -ne 0) {
  Write-Host "Upload failed. Hold BOOT, tap RESET, release BOOT, run again." -ForegroundColor Yellow
  exit $LASTEXITCODE
}

$binOut = Join-Path $root "docs\firmware-s3.bin"
$built = Get-ChildItem "$env:LOCALAPPDATA\arduino\sketches" -Recurse -Filter "ESP32_Control.ino.bin" |
  Sort-Object LastWriteTime -Descending | Select-Object -First 1
if ($built) {
  Copy-Item $built.FullName $binOut -Force
  Write-Host "Copied $($built.Length) bytes -> docs/firmware-s3.bin"
}

Write-Host ""
Write-Host "FACTORY RESET COMPLETE" -ForegroundColor Green
Write-Host "Firmware v3.0.0 | WiFi: mikrotik + kalithea | BMS: JK + Basen | Genset: PS0600"
Write-Host "Serial Monitor @ 115200, then http://esp32.local or check router DHCP"
