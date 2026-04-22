# Capture HB lines from ST-Link VCP. Requires Windows PowerShell (.NET Framework) for SerialPort.
param(
    [string]$Port = "COM10",
    [int]$Baud = 115200,
    [int]$Seconds = 12,
    [int]$MaxLines = 12
)
$ErrorActionPreference = "Stop"
$p = New-Object System.IO.Ports.SerialPort($Port, $Baud, [System.IO.Ports.Parity]::None, 8, [System.IO.Ports.StopBits]::One)
$p.NewLine = "`r`n"
$p.ReadTimeout = 4000
$p.Open()
try {
    $deadline = (Get-Date).AddSeconds($Seconds)
    $count = 0
    while (((Get-Date) -lt $deadline) -and ($count -lt $MaxLines)) {
        try {
            $line = $p.ReadLine()
            Write-Output $line
            $count++
        }
        catch [System.TimeoutException] {
            Start-Sleep -Milliseconds 100
        }
    }
}
finally {
    $p.Close()
}
