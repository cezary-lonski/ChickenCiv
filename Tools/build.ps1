# Buduje projekt. Uzycie: powershell -File Tools\build.ps1 [-Target ChickenCivEditor] [-Config Development]
param([string]$Target = 'ChickenCivEditor', [string]$Config = 'Development')
. $PSScriptRoot\env.ps1
if (-not (Test-Path $Proj)) { Write-Host "BRAK PROJEKTU: $Proj"; exit 1 }
# UBT bywa slepy na nowe pliki: gdy liczba plikow zrodlowych sie zmieni, dotykamy Build.cs.
$src = (Get-ChildItem (Join-Path $Root 'Source') -Recurse -Include *.cpp,*.h,*.cs | Measure-Object).Count
$stamp = Join-Path $Root 'Saved\srccount.txt'
$old = if (Test-Path $stamp) { Get-Content $stamp } else { '' }
if ("$old" -ne "$src") { (Get-Item (Join-Path $Root 'Source\ChickenCiv\ChickenCiv.Build.cs')).LastWriteTime = Get-Date; New-Item -ItemType Directory -Force (Split-Path $stamp) | Out-Null; Set-Content $stamp $src }
& $Build $Target Win64 $Config "-Project=$Proj" -WaitMutex -NoHotReload
exit $LASTEXITCODE
