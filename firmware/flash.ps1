<#
  flash.ps1 - build + flash DOOM to the ESP32-S3 business card.

  Usage (from a PowerShell prompt inside the Firmware folder):
      .\flash.ps1                 # auto-detect the COM port, do everything
      .\flash.ps1 -Port COM7      # force a specific port
      .\flash.ps1 -WadOnly        # only re-flash the WAD (skip rebuilding app)
      .\flash.ps1 -AppOnly        # only build+flash the app (skip the WAD)

  It does two things:
    1. Builds the Doom firmware and flashes it (arduino-cli).
    2. Packs Firmware\data\DOOM1.WAD into a LittleFS image and flashes it
       to the 'spiffs' partition at offset 0x210000.
#>
param(
  [string]$Port = "",
  [switch]$WadOnly,
  [switch]$AppOnly
)
$ErrorActionPreference = "Stop"
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $ScriptDir

$FQBN = "esp32:esp32:esp32s3:PSRAM=opi,FlashSize=8M,PartitionScheme=custom,CDCOnBoot=cdc,FlashMode=qio,CPUFreq=240"
$CFLAGS  = "-DDOOMGENERIC_RESX=320 -DDOOMGENERIC_RESY=200 -Wno-error"
$CXXFLAGS = "-DDOOMGENERIC_RESX=320 -DDOOMGENERIC_RESY=200"
$LFS_OFFSET = "0x210000"
$LFS_SIZE   = 6160384          # 0x5E0000, must match partitions.csv 'spiffs'

# ---- locate tools shipped with the ESP32 core --------------------------------
$ToolsRoot = Join-Path $env:LOCALAPPDATA "Arduino15\packages\esp32\tools"
function Find-Newest($subdir, $pattern) {
  $hit = Get-ChildItem -Path (Join-Path $ToolsRoot $subdir) -Recurse -Filter $pattern -ErrorAction SilentlyContinue |
         Sort-Object FullName -Descending | Select-Object -First 1
  if (-not $hit) { throw "Could not find $pattern under $subdir. Is the esp32 core installed?" }
  return $hit.FullName
}
$mklittlefs = Find-Newest "mklittlefs" "mklittlefs.exe"
$esptool    = Find-Newest "esptool_py" "esptool.exe"

# ---- pick a port -------------------------------------------------------------
if (-not $Port) {
  $line = (arduino-cli board list) | Select-String "esp32|Serial|USB|COM" | Select-Object -First 1
  if ($line -and $line.ToString() -match "(COM\d+)") { $Port = $Matches[1] }
}
if (-not $Port) { throw "No COM port found. Plug in the board and/or pass -Port COMx." }
Write-Host "Using port: $Port" -ForegroundColor Cyan

# ---- 1. build + flash the app ------------------------------------------------
if (-not $WadOnly) {
  Write-Host "`n=== Building + flashing firmware ===" -ForegroundColor Green
  arduino-cli compile -b $FQBN `
    --build-property "compiler.c.extra_flags=$CFLAGS" `
    --build-property "compiler.cpp.extra_flags=$CXXFLAGS" `
    -u -p $Port "$ScriptDir"
  if ($LASTEXITCODE -ne 0) { throw "Firmware build/upload failed." }
}

# ---- 2. pack + flash the WAD -------------------------------------------------
if (-not $AppOnly) {
  $wad = Join-Path $ScriptDir "data\DOOM1.WAD"
  if (-not (Test-Path $wad)) { throw "Missing $wad - put DOOM1.WAD in the data folder." }
  Write-Host "`n=== Packing LittleFS image ===" -ForegroundColor Green
  $img = Join-Path $ScriptDir "littlefs.bin"
  & $mklittlefs -c (Join-Path $ScriptDir "data") -p 256 -b 4096 -s $LFS_SIZE $img
  if ($LASTEXITCODE -ne 0) { throw "mklittlefs failed." }

  Write-Host "`n=== Flashing WAD to $LFS_OFFSET ===" -ForegroundColor Green
  & $esptool --chip esp32s3 -p $Port --baud 921600 write_flash $LFS_OFFSET $img
  if ($LASTEXITCODE -ne 0) { throw "esptool write_flash failed." }
}

Write-Host "`nDone. Open the serial monitor at 115200 baud to watch it boot." -ForegroundColor Cyan
