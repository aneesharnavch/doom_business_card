@echo off
REM ============================================================
REM  Double-click this to flash DOOM to the ESP32-S3 card.
REM  It builds + flashes the engine AND the WAD in one go.
REM ============================================================
setlocal
cd /d "%~dp0"

echo.
echo  ====================================================
echo   Flashing DOOM (engine + WAD) to the ESP32-S3 card
echo  ====================================================
echo.
echo  Make sure the board is plugged in via USB.
echo.
pause

powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0flash.ps1"

echo.
echo  ----------------------------------------------------
if %ERRORLEVEL%==0 (
  echo   DONE. DOOM is on the board. Have fun.
) else (
  echo   Something failed ^(see messages above^).
  echo   Tip: hold BOOT, tap RESET, release BOOT, then try again.
)
echo  ----------------------------------------------------
echo.
pause
endlocal
