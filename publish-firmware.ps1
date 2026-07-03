# Compile firmware and publish .bin to docs/ for remote MQTT OTA.
$cli = "$env:LOCALAPPDATA\arduino-cli\arduino-cli.exe"
$sketch = Join-Path $PSScriptRoot "ESP32_Control"
$fqbn = "esp32:esp32:esp32:PartitionScheme=min_spiffs,FlashSize=4M"
$out = Join-Path $PSScriptRoot "docs\firmware.bin"
$buildRoot = Join-Path $env:LOCALAPPDATA "arduino\sketches"

Write-Host "=== Publish firmware for remote OTA ===" -ForegroundColor Cyan
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
Write-Host "Copied to docs/firmware.bin ($mb MB)" -ForegroundColor Green
Write-Host ""
Write-Host "Next: git add docs/firmware.bin && git commit && git push"
Write-Host "Then trigger OTA from GitHub Pages or MQTT:"
Write-Host '  {"ota":"esp32ota"}'
