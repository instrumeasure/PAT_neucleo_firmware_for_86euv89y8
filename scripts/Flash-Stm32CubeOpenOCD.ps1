#Requires -Version 5.1
param(
  [string]$Elf = ""
)
$ErrorActionPreference = "Stop"
$RepoRoot = Split-Path -Parent $PSScriptRoot
if (-not $Elf) {
  $Elf = Join-Path $RepoRoot "cmake-build\pat_nucleo_h753.elf"
}

if (-not (Test-Path $Elf)) {
  Write-Error "ELF not found: $Elf — run Build-Stm32CubeCMake.ps1 first"
}

$openocdExe = $null
$cmdOcd = Get-Command openocd -ErrorAction SilentlyContinue
if ($cmdOcd) { $openocdExe = $cmdOcd.Path }
if (-not $openocdExe) {
  $pioOcd = Join-Path $env:USERPROFILE ".platformio\packages\tool-openocd\bin\openocd.exe"
  if (Test-Path $pioOcd) { $openocdExe = $pioOcd }
}
if (-not $openocdExe) { Write-Error "openocd not on PATH and not under PlatformIO packages" }

$elfAbs = (Resolve-Path $Elf).Path
$elfEsc = $elfAbs -replace '\\','/'
$cmd = "program `"$elfEsc`" verify reset exit"
& $openocdExe -f interface/stlink.cfg -f target/stm32h7x.cfg -c $cmd
