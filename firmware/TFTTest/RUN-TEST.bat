@echo off
REM ============================================================
REM  Double-click this FIRST (before flashing the game).
REM  Builds + flashes the display/button bring-up test.
REM ============================================================
setlocal
cd /d "%~dp0"

echo.
echo  ====================================================
echo   Flashing the TFT + button TEST to the ESP32-S3 card
echo  ====================================================
echo.
echo  Make sure the board is plugged in via USB.
echo.
pause

powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0flash-test.ps1"

echo.
echo  ----------------------------------------------------
if %ERRORLEVEL%==0 (
  echo   DONE. Watch the screen: color bars, then press
  echo   each button and check its label lights up green.
) else (
  echo   Something failed ^(see messages above^).
  echo   Tip: hold BOOT, tap RESET, release BOOT, then try again.
)
echo  ----------------------------------------------------
echo.
pause
endlocal
