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
  # Optional: halt on ADS127 bring-up / post-START gate failure (default OFF = warn and continue):
  #   cmake ... -DPAT_ADS127_STRICT_BRINGUP=ON
  # Quartet (see AGENTS): `pat_nucleo_quartet` always uses shared !CS + parallel DRDY epoch (CMake).
  # Optional overrides, e.g. `-DPAT_QUARTET_PARALLEL_SPI4_DRDY_ONLY=OFF` for all-MISO DRDY wait.
  # Optional TI SBAS946 §8.5.9 3-wire SPI (all !CS held low): add
  #   "-DPAT_ADS127_SPI_3WIRE_CS_HELD_LOW=ON" `
  # Throughput / LA correlate (quartet README § epoch vs ODR), e.g.:
  #   "-DPAT_SPI123_PRESCALER_DIV=32" `
  #   "-DPAT_QUARTET_SPI_IRQ_PREEMPT_PRIORITY=5" `
  #   "-DPAT_QUARTET_DIAG_EPOCH_EVERY=ON" `
  #   "-DPAT_QUARTET_PARALLEL_SPI_RX3_SEQ_UNLOCKED=ON" `
  cmake -G Ninja `
    "-DCMAKE_TOOLCHAIN_FILE=$toolchain" `
    "-DSTM32_CUBE_H7_FW=$($env:STM32_CUBE_H7_FW)" `
    "-DPAT_QUARTET_PARALLEL_SPI4_DRDY_ONLY=ON" `
    $RepoRoot
  if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
  cmake --build .
  if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}
finally {
  Pop-Location
}

Write-Host "OK: $(Join-Path $buildDir 'pat_nucleo_h753.elf')"
