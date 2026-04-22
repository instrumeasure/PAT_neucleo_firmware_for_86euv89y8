# Flash pat_nucleo_h753.elf using OpenOCD (same tool PlatformIO uses). Requires ST-Link + OpenOCD on PATH.
param(
    [string]$Elf = ""
)
$ErrorActionPreference = "Stop"
$repo = Split-Path $PSScriptRoot -Parent
if (-not $Elf) {
    $bd = Join-Path $repo "cmake-build"
    $Elf = Join-Path $bd "pat_nucleo_h753.elf"
    if (-not (Test-Path $Elf)) {
        $Elf = Join-Path $bd "pat_nucleo_h753"
    }
}
if (-not (Test-Path $Elf)) {
    Write-Error "Build ELF not found - run scripts\Build-Stm32CubeCMake.ps1 first."
}
$openocdExe = $null
$cmd = Get-Command openocd.exe -ErrorAction SilentlyContinue
if ($cmd) { $openocdExe = $cmd.Source }
if (-not $openocdExe) {
    $pioOpenocd = Join-Path $env:USERPROFILE ".platformio\packages\tool-openocd\bin\openocd.exe"
    if (Test-Path $pioOpenocd) { $openocdExe = $pioOpenocd }
}
if (-not $openocdExe) {
    Write-Error "openocd not on PATH and not found under PlatformIO packages."
}
$elfArg = $Elf -replace '\\', '/'
$ocdCmd = ('program "{0}" verify reset exit' -f $elfArg)
& $openocdExe -f interface/stlink.cfg -f target/stm32h7x.cfg -c $ocdCmd
