# Creates a directory junction: <repo>\framework-stm32cubeh7 -> PlatformIO's framework pack.
# Required for STM32-for-VSCode (bmd) to resolve HAL/CMSIS paths in STM32-for-VSCode.config.yaml
# without checking in the full ST pack. Run once per clone:  powershell -File scripts\Ensure-STM32CubeFrameworkLink.ps1
$ErrorActionPreference = "Stop"
$repoRoot = Split-Path $PSScriptRoot -Parent
$target = Join-Path $env:USERPROFILE ".platformio\packages\framework-stm32cubeh7"
$link = Join-Path $repoRoot "framework-stm32cubeh7"
if (-not (Test-Path $target)) {
    Write-Error "PlatformIO framework not found at $target. Run: py -m platformio run (or: py -m platformio pkg install -p ststm32)"
    exit 1
}
if (Test-Path $link) {
    $i = Get-Item $link
    if ($i.LinkType) { Write-Host "OK: $link already points to framework."; exit 0 }
    Write-Error "Path exists and is not a link: $link. Remove it or rename, then re-run."
    exit 1
}
cmd /c "mklink /J `"$link`" `"$target`""
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
Write-Host "Created junction: $link -> $target"
