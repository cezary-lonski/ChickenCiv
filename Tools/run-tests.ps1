# Uruchamia testy automatyczne bez okna. Uzycie: powershell -File Tools\run-tests.ps1 [-Filter ChickenCiv] [-NoBuild]
# Kod 0 tylko, gdy testy przeszly i jakies sie uruchomily.
param([string]$Filter = 'ChickenCiv', [switch]$NoBuild)
. $PSScriptRoot\env.ps1
if (-not $NoBuild) {
    $blog = Join-Path $Root 'Saved\Logs\build.log'
    New-Item -ItemType Directory -Force -Path (Split-Path $blog) | Out-Null
    & $PSScriptRoot\build.ps1 *> $blog
    if ($LASTEXITCODE -ne 0) { Write-Host 'BLAD KOMPILACJI:'; Select-String -Path $blog -Pattern 'error' | Select-Object -First 15 | ForEach-Object { $_.Line }; exit 1 }
}
$log = Join-Path $Root 'Saved\Logs\tests.log'
New-Item -ItemType Directory -Force -Path (Split-Path $log) | Out-Null
& $Cmd $Proj "-ExecCmds=Automation RunTests $Filter; Quit" -unattended -nullrhi -nosplash -NoSound "-abslog=$log" -stdout -FullStdOutLogOutput | Out-Null
$text = Get-Content $log -Raw -ErrorAction SilentlyContinue
$failed = ([regex]::Matches($text, 'Test Completed\. Result=\{Fail')).Count
$passed = ([regex]::Matches($text, 'Test Completed\. Result=\{Success')).Count
Write-Host "Testy: $passed zaliczone, $failed niezaliczone"
if ($failed -gt 0) { Select-String -Path $log -Pattern 'Result=\{Fail|Error:' | Select-Object -First 20 | ForEach-Object { $_.Line } }
if ($failed -gt 0 -or $passed -eq 0) { exit 1 }
exit 0
