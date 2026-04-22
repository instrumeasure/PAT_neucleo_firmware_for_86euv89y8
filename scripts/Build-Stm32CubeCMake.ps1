# Build firmware with CMake + STM32Cube HAL pack (official layout; no PlatformIO required to build).
# Prerequisites: CMake, Ninja (install: winget install Ninja-build.Ninja), arm-none-eabi-gcc on PATH.
# Firmware pack: STM32_CUBE_H7_FW env or PlatformIO framework copy (after one `py -m platformio run`).
$ErrorActionPreference = "Stop"
$repo = Resolve-Path (Join-Path $PSScriptRoot "..")
$buildDir = Join-Path $repo "cmake-build"
$fw = $env:STM32_CUBE_H7_FW
if (-not $fw) {
    $fw = Join-Path $env:USERPROFILE ".platformio\packages\framework-stm32cubeh7"
}
if (-not (Test-Path (Join-Path $fw "Drivers\CMSIS\Include\cmsis_compiler.h"))) {
    Write-Error @"
STM32Cube H7 firmware pack not found at:
  $fw
Either:
  Run once: py -m platformio run -e nucleo_h753zi
  Or download STM32Cube FW STM32Cube FW_H7 from ST and set:
  `$env:STM32_CUBE_H7_FW = 'C:\path\to\STM32Cube_FW_H7'
"@ 
}
$tc = (Join-Path $repo "cmake\gcc-arm-none-eabi.cmake").Replace('\', '/')
$fwFwd = $fw.Replace('\', '/')

# Refresh PATH so Ninja is visible (after winget install).
$env:Path = [System.Environment]::GetEnvironmentVariable("Path", "Machine") + ";" + [System.Environment]::GetEnvironmentVariable("Path", "User")

$cmakeArgs = @(
    "-B", $buildDir,
    "-G", "Ninja",
    "-DCMAKE_TOOLCHAIN_FILE=$tc",
    "-DSTM32_CUBE_H7_FW=$fwFwd",
    "-S", $repo
)
Write-Host "cmake $($cmakeArgs -join ' ')"
& cmake @cmakeArgs
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& cmake --build $buildDir
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
Write-Host ""
Write-Host "Outputs: $buildDir\pat_nucleo_h753.elf , pat_nucleo_h753.bin"
