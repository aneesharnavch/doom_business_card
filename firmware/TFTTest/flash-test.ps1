<#
  flash-test.ps1 - build + flash the TFTTest bring-up sketch.
  Confirms display pins, orientation, PSRAM, and all 5 buttons + chords.

  Usage:
      .\flash-test.ps1            # auto-detect the COM port
      .\flash-test.ps1 -Port COM7
#>
param([string]$Port = "")
$ErrorActionPreference = "Stop"
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $ScriptDir

$FQBN = "esp32:esp32:esp32s3:PSRAM=opi,FlashSize=8M,CDCOnBoot=cdc"

# pick a port
if (-not $Port) {
  $line = (arduino-cli board list) | Select-String "esp32|Serial|USB|COM" | Select-Object -First 1
  if ($line -and $line.ToString() -match "(COM\d+)") { $Port = $Matches[1] }
}
if (-not $Port) { throw "No COM port found. Plug in the board and/or pass -Port COMx." }
Write-Host "Using port: $Port" -ForegroundColor Cyan

Write-Host "`n=== Building + flashing TFTTest ===" -ForegroundColor Green
arduino-cli compile -b $FQBN -u -p $Port "$ScriptDir"
if ($LASTEXITCODE -ne 0) { throw "Build/upload failed." }

Write-Host "`nDone. Open the serial monitor at 115200 baud:" -ForegroundColor Cyan
Write-Host "  arduino-cli monitor -p $Port -c baudrate=115200"
