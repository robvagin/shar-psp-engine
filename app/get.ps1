# Одна строка для Windows (в PowerShell):
#   irm https://raw.githubusercontent.com/robvagin/shar-psp-engine/main/app/get.ps1 | iex
# Сборка идёт в Ubuntu внутри Windows (WSL): её нет — ставит (нужен перезапуск компьютера, потом та же строка ещё раз).
# Страница-сборщик открывается в браузере Windows: WSL2 отдаёт 127.0.0.1 в Windows.
$ErrorActionPreference = 'Stop'
if (-not (Get-Command wsl.exe -ErrorAction SilentlyContinue)) {
  Write-Host 'SHAR for PSP needs Windows 10 (version 2004 or newer) or Windows 11: this one has no WSL (Ubuntu inside Windows). Update Windows and run the same line again'
  return
}
# имя дистрибутива берём как есть: «Ubuntu», «Ubuntu-24.04», «Ubuntu-22.04»; wsl -d Ubuntu не находит «Ubuntu-24.04»
$have = (wsl.exe -l -q 2>$null) -replace "`0", '' | ForEach-Object { $_.Trim() } | Where-Object { $_ -match '^Ubuntu' } | Select-Object -First 1
if (-not $have) {
  Write-Host 'SHAR for PSP: installing Ubuntu for Windows (WSL). Windows will ask for permission; restart the computer when it finishes, then run the same line again'
  Write-Host 'If it says virtualization is off: turn on "Virtualization" (VT-x / SVM) in the BIOS, then run the same line again'
  Start-Process wsl.exe -ArgumentList '--install','-d','Ubuntu' -Verb RunAs -Wait
  return
}
Write-Host "SHAR for PSP: starting the builder in $have (WSL). Keep this window open"
wsl.exe -d $have -- bash -lc 'curl -fsSL https://raw.githubusercontent.com/robvagin/shar-psp-engine/main/app/get.sh | bash'
