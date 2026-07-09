param([string]$Port = "auto")

$cli = "$env:LOCALAPPDATA\arduino-cli\arduino-cli.exe"
$sketch = Join-Path $PSScriptRoot "ESP32_Control"
$fqbn = "esp32:esp32:esp32s3:CDCOnBoot=cdc,USBMode=hwcdc,FlashSize=16M,PartitionScheme=min_spiffs,PSRAM=opi,UploadSpeed=921600"

Write-Host "=== ESP32-S3 Upload (Waveshare RS485-CAN) ===" -ForegroundColor Cyan

if ($Port -eq "auto") {
  $ports = & $cli board list | Select-String "serial|usb|jtag|cdc" -CaseSensitive:$false
  if (-not $ports) {
    Write-Host ""
    Write-Host "ERROR: ESP32-S3 not found on USB." -ForegroundColor Red
    Write-Host "1. Use USB-C data cable to the Type-C port"
    Write-Host "2. Hold BOOT, tap RESET, release BOOT if upload fails"
    Write-Host "3. Run: .\upload-s3.ps1 -Port COMx"
    exit 1
  }
}

Write-Host "Compiling for ESP32-S3..."
& $cli compile --fqbn $fqbn $sketch
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host "Uploading to $Port..."
& $cli upload -p $Port --fqbn $fqbn $sketch
if ($LASTEXITCODE -ne 0) {
    Write-Host ""
    Write-Host "Upload failed. Hold BOOT and run again." -ForegroundColor Yellow
    exit $LASTEXITCODE
}

Write-Host ""
Write-Host "SUCCESS! Open Serial Monitor @ 115200, then http://<device-ip>" -ForegroundColor Green
Write-Host "RS485: wire A+/B- on screw terminals directly to PS0600 TB15."
