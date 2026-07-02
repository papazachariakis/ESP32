# Run as Administrator: Right-click -> Run with PowerShell (as Admin)
$inf = "$env:USERPROFILE\Downloads\CP210x_Universal_Windows_Driver\silabser.inf"

if (-not (Test-Path $inf)) {
    Write-Host "Driver not found at: $inf" -ForegroundColor Red
    Write-Host "Download from: https://www.silabs.com/developers/usb-to-uart-bridge-vcp-drivers"
    exit 1
}

Write-Host "Installing CP210x driver..." -ForegroundColor Cyan
pnputil /add-driver $inf /install

if ($LASTEXITCODE -eq 0) {
    Write-Host ""
    Write-Host "Driver installed! Unplug and replug ESP32 USB cable." -ForegroundColor Green
    Write-Host "Then run: .\upload.ps1"
} else {
    Write-Host ""
    Write-Host "Or install manually:" -ForegroundColor Yellow
    Write-Host "Right-click silabser.inf -> Install"
}
