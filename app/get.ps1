# Одна строка для Windows (в PowerShell):
#   irm https://raw.githubusercontent.com/robvagin/shar-psp-engine/main/app/get.ps1 | iex
# Сборка идёт в Ubuntu внутри Windows (WSL): её нет — ставит (нужен перезапуск компьютера, потом та же строка ещё раз).
# Страница-сборщик открывается в браузере Windows: WSL2 отдаёт 127.0.0.1 в Windows.
$ErrorActionPreference = 'Stop'
$have = (wsl.exe -l -q 2>$null) -replace "`0", '' | Where-Object { $_ -match 'Ubuntu' }
if (-not $have) {
  Write-Host 'SHAR for PSP: installing Ubuntu for Windows (WSL). Windows will ask for permission; restart the computer when it finishes, then run the same line again'
  Start-Process wsl.exe -ArgumentList '--install','-d','Ubuntu' -Verb RunAs -Wait
  return
}
Write-Host 'SHAR for PSP: starting the builder in Ubuntu (WSL). Keep this window open'
wsl.exe -d Ubuntu -- bash -lc 'curl -fsSL https://raw.githubusercontent.com/robvagin/shar-psp-engine/main/app/get.sh | bash'
