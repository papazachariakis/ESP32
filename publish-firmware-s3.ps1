# Compile ESP32-S3 firmware and publish .bin to docs/ for remote OTA.
$cli = "$env:LOCALAPPDATA\arduino-cli\arduino-cli.exe"
$sketch = Join-Path $PSScriptRoot "ESP32_Control"
$fqbn = "esp32:esp32:esp32s3:CDCOnBoot=cdc,USBMode=hwcdc,FlashSize=16M,PartitionScheme=min_spiffs,PSRAM=opi"
$out = Join-Path $PSScriptRoot "docs\firmware-s3.bin"
$buildRoot = Join-Path $env:LOCALAPPDATA "arduino\sketches"

Write-Host "=== Publish S3 firmware for remote OTA ===" -ForegroundColor Cyan
& $cli compile --fqbn $fqbn $sketch
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

$binFile = $null
Get-ChildItem $buildRoot -Directory | Sort-Object LastWriteTime -Descending | ForEach-Object {
    if ($binFile) { return }
    $f = Join-Path $_.FullName "ESP32_Control.ino.bin"
    if (Test-Path $f) { $binFile = $f }
}
if (-not $binFile) {
    Write-Host "ERROR: .bin not found" -ForegroundColor Red
    exit 1
}

Copy-Item $binFile $out -Force
$mb = [math]::Round((Get-Item $out).Length / 1MB, 2)
Write-Host "Copied to docs/firmware-s3.bin ($mb MB)" -ForegroundColor Green
Write-Host ""
Write-Host "Next: git add docs/firmware-s3.bin docs/s3.html && git commit && git push"
Write-Host "Then OTA from s3.html (browser push) or MQTT: {\"ota\":\"esp32ota\"}"
