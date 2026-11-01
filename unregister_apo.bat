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
echo Unregistering SonicBoost Audio Processing Object (APO)
echo ========================================================

echo Restoring original audio endpoints...
SonicBoost.exe /uninstall

echo Unregistering APO DLL...
regsvr32.exe /u /s SoundBoosterApo.dll

echo.
echo ========================================================
echo SonicBoost APO uninstalled and audio endpoints restored.
echo ========================================================
pause
