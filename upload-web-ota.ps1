param([string]$OtaHost = "192.168.99.63")

$cli = "$env:LOCALAPPDATA\arduino-cli\arduino-cli.exe"
$sketch = Join-Path $PSScriptRoot "ESP32_Control"
$fqbn = "esp32:esp32:esp32:PartitionScheme=min_spiffs,FlashSize=4M"
$bin = Join-Path $env:LOCALAPPDATA "arduino\sketches"

Write-Host "=== ESP32 Web OTA Upload ===" -ForegroundColor Cyan
Write-Host "Target: http://$OtaHost/api/ota"
Write-Host ""

Write-Host "Compiling..."
& $cli compile --fqbn $fqbn $sketch
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

$buildDirs = Get-ChildItem $bin -Directory | Sort-Object LastWriteTime -Descending
$binFile = $null
foreach ($d in $buildDirs) {
    $f = Join-Path $d.FullName "ESP32_Control.ino.bin"
    if (Test-Path $f) { $binFile = $f; break }
}
if (-not $binFile) {
    Write-Host "ERROR: .bin not found after compile" -ForegroundColor Red
    exit 1
}

Write-Host "Uploading $([math]::Round((Get-Item $binFile).Length/1MB,2)) MB via HTTP..."
$resp = curl.exe -s -m 300 -F "firmware=@$binFile" "http://$OtaHost/api/ota"
Write-Host $resp
if ($resp -notmatch '"ok"\s*:\s*true') {
    Write-Host ""
    Write-Host "Web OTA failed. Try:" -ForegroundColor Yellow
    Write-Host "  .\upload-ota.ps1 -OtaHost $OtaHost"
    Write-Host "  .\upload.ps1 -Port COM5"
    exit 1
}

Write-Host ""
Write-Host "SUCCESS! Device rebooting..." -ForegroundColor Green
