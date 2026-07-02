$cli = "$env:LOCALAPPDATA\arduino-cli\arduino-cli.exe"

Write-Host "Serial Monitor (115200 baud). Press Ctrl+C to exit." -ForegroundColor Cyan
& $cli monitor -p auto -c baudrate=115200
