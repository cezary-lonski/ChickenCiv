# Sciezki srodowiska. Ladowane przez inne skrypty: . $PSScriptRoot\env.ps1
$UE    = 'D:\Gry\Epic Games\UE_5.8'
$Root  = Split-Path $PSScriptRoot -Parent
$Proj  = Join-Path $Root 'ChickenCiv.uproject'
$Cmd   = Join-Path $UE 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$Editor = Join-Path $UE 'Engine\Binaries\Win64\UnrealEditor.exe'
$Build = Join-Path $UE 'Engine\Build\BatchFiles\Build.bat'
$UAT   = Join-Path $UE 'Engine\Build\BatchFiles\RunUAT.bat'
# Cache i pliki tymczasowe UE na D, nie na C.
$env:UE_LocalDataCachePath = 'D:\UEDerivedDataCache'
$env:TEMP = 'D:\Temp'; $env:TMP = 'D:\Temp'
New-Item -ItemType Directory -Force -Path 'D:\Temp' | Out-Null
