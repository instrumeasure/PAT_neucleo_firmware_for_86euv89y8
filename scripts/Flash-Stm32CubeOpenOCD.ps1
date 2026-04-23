#Requires -Version 5.1

# Default image (no args): pat_nucleo_h753 - single SPI4 + one ADS127 (main.c).

# Four SPI1-4 + four ADS127:  -Quartet   or   -Elf cmake-build\pat_nucleo_quartet.elf

# SPI6 J2 smoke:             -Spi6      or   -Elf cmake-build\pat_nucleo_spi6.elf

# SPI1-4 net check (single bus/phase):  -Spi1_4  or  -Spi123 (alias)  or  -Elf ...\pat_nucleo_spi1_4_scan.elf

# One ADS127 on SPI N only (separate ELFs):  -SingleSpi 1  (..2..3..4)  or  -Elf ...\pat_nucleo_spiN_ads127.elf

param(

  [string]$Elf = "",

  [switch]$Quartet,

  [switch]$Spi6,

  [switch]$Spi123,

  [switch]$Spi1_4,

  [string]$SingleSpi = ""

)

$ErrorActionPreference = "Stop"

$RepoRoot = Split-Path -Parent $PSScriptRoot

$sel = 0

if ($Quartet) { $sel++ }

if ($Spi6) { $sel++ }

if ($Spi123) { $sel++ }

if ($Spi1_4) { $sel++ }

if ($SingleSpi -ne "") { $sel++ }

if ($sel -gt 1) {

  Write-Error "Use only one of -Quartet, -Spi6, -Spi1_4, -Spi123 (alias of -Spi1_4), or -SingleSpi."

}

if ($Quartet) {

  $Elf = Join-Path $RepoRoot "cmake-build\pat_nucleo_quartet.elf"

} elseif ($Spi6) {

  $Elf = Join-Path $RepoRoot "cmake-build\pat_nucleo_spi6.elf"

} elseif ($Spi123 -or $Spi1_4) {

  $Elf = Join-Path $RepoRoot "cmake-build\pat_nucleo_spi1_4_scan.elf"

} elseif ($SingleSpi -ne "") {

  if ($SingleSpi -notmatch '^[1-4]$') {

    Write-Error "-SingleSpi must be 1, 2, 3, or 4 (pat_nucleo_spiN_ads127)."

  }

  $Elf = Join-Path $RepoRoot "cmake-build\pat_nucleo_spi${SingleSpi}_ads127.elf"

} elseif (-not $Elf) {

  $ElfElf = Join-Path $RepoRoot "cmake-build\pat_nucleo_h753.elf"

  $ElfBare = Join-Path $RepoRoot "cmake-build\pat_nucleo_h753"

  if (Test-Path $ElfElf) {

    $Elf = $ElfElf

  } elseif (Test-Path $ElfBare) {

    $Elf = $ElfBare

  } else {

    $Elf = $ElfElf

  }

}



if ($Elf) {

  if (-not [System.IO.Path]::IsPathRooted($Elf)) {

    $Elf = Join-Path $RepoRoot $Elf

  }

  # arm-none-eabi often emits pat_nucleo_* with no .elf suffix; accept either name.

  if (-not (Test-Path -LiteralPath $Elf)) {

    $bare = if ($Elf.EndsWith(".elf")) { $Elf.Substring(0, $Elf.Length - 4) } else { $null }

    $withElf = if ($Elf.EndsWith(".elf")) { $Elf } else { "$Elf.elf" }

    if ($bare -and (Test-Path -LiteralPath $bare)) {

      $Elf = $bare

    } elseif (Test-Path -LiteralPath $withElf) {

      $Elf = $withElf

    }

  }

}



if (-not (Test-Path -LiteralPath $Elf)) {

  Write-Error "Image not found: $Elf - run Build-Stm32CubeCMake.ps1. Use -Quartet, -Spi6, -Spi1_4 (-Spi123 alias), -SingleSpi 1..4, or -Elf with an explicit path."

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

Write-Host "OpenOCD program: $elfAbs"

$elfEsc = $elfAbs -replace '\\', '/'

$cmd = 'program "' + $elfEsc + '" verify reset exit'

& $openocdExe -f interface/stlink.cfg -f target/stm32h7x.cfg -c $cmd

if ($LASTEXITCODE -ne 0) {

  exit $LASTEXITCODE

}

