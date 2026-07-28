# Start PVX remote bridge + Cloudflare quick tunnel
$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
Set-Location $root

Write-Host "Starting PVX bridge on :8767 ..."
Start-Process -WindowStyle Minimized python -ArgumentList "tools/pvx_remote_bridge.py"

Start-Sleep -Seconds 3
$cloud = "C:\Program Files (x86)\cloudflared\cloudflared.exe"
if (-not (Test-Path $cloud)) { $cloud = "cloudflared" }

Write-Host "Starting Cloudflare quick tunnel ..."
& $cloud tunnel --url http://127.0.0.1:8767 --no-autoupdate
