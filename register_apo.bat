@echo off
setlocal
cd /d "%~dp0"

:: Check for Administrative Privileges and auto-elevate if needed
net session >nul 2>&1
if %errorLevel% neq 0 (
    echo [INFO] Requesting Administrative Privileges...
    powershell -NoProfile -ExecutionPolicy Bypass -Command "Start-Process -FilePath '%~f0' -Verb RunAs"
    exit /b
)

echo ========================================================
echo Installing SonicBoost Audio Processing Object (APO)
echo ========================================================

echo Registering APO DLL...
regsvr32.exe /s SoundBoosterApo.dll

echo Installing APO to active Audio Render Endpoints...
SonicBoost.exe /install

echo.
echo ========================================================
echo Installation Finished!
echo SonicBoost APO is now actively processing system audio.
echo ========================================================
pause
