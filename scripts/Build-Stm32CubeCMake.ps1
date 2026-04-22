#Requires -Version 5.1
$ErrorActionPreference = "Stop"
$RepoRoot = Split-Path -Parent $PSScriptRoot

# Refresh PATH (PIO toolchain + optional local ninja)
$pioGcc = Join-Path $env:USERPROFILE ".platformio\packages\toolchain-gccarmnoneeabi\bin"
if (Test-Path $pioGcc) {
  $env:PATH = "$pioGcc;$env:PATH"
}
$localNinja = Join-Path $RepoRoot "tools"
if (Test-Path (Join-Path $localNinja "ninja.exe")) {
  $env:PATH = "$localNinja;$env:PATH"
}
$ninjaWinget = Join-Path $env:LOCALAPPDATA "Microsoft\WinGet\Packages\Ninja-build.Ninja_Microsoft.Winget.Source_8wekyb3d8bbwe"
if (Test-Path (Join-Path $ninjaWinget "ninja.exe")) {
  $env:PATH = "$ninjaWinget;$env:PATH"
}

foreach ($cmakeBin in @(
    "C:\Program Files\CMake\bin",
    "C:\Program Files (x86)\CMake\bin",
    (Join-Path $env:LOCALAPPDATA "Programs\CMake\bin")
  )) {
  if (Test-Path (Join-Path $cmakeBin "cmake.exe")) {
    $env:PATH = "$cmakeBin;$env:PATH"
    break
  }
}

if (-not $env:STM32_CUBE_H7_FW) {
  $env:STM32_CUBE_H7_FW = Join-Path $env:USERPROFILE ".platformio\packages\framework-stm32cubeh7"
}
if (-not (Test-Path (Join-Path $env:STM32_CUBE_H7_FW "Drivers\CMSIS\Include\cmsis_compiler.h"))) {
  Write-Error "STM32_CUBE_H7_FW invalid: $($env:STM32_CUBE_H7_FW) (missing cmsis_compiler.h)"
}

$buildDir = Join-Path $RepoRoot "cmake-build"
if (-not (Test-Path $buildDir)) { New-Item -ItemType Directory -Path $buildDir | Out-Null }

Push-Location $buildDir
try {
  $toolchain = Join-Path $RepoRoot "cmake\gcc-arm-none-eabi.cmake"
  cmake -G Ninja `
    "-DCMAKE_TOOLCHAIN_FILE=$toolchain" `
    "-DSTM32_CUBE_H7_FW=$($env:STM32_CUBE_H7_FW)" `
    $RepoRoot
  if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
  cmake --build .
  if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}
finally {
  Pop-Location
}

Write-Host "OK: $(Join-Path $buildDir 'pat_nucleo_h753.elf')"
