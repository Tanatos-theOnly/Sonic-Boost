@echo off
setlocal
echo ========================================================
echo Unregistering SonicBoost Audio Processing Object (APO)
echo ========================================================
cd /d "%~dp0"

echo Restoring original audio endpoints...
SonicBoost.exe /uninstall

echo Unregistering APO DLL...
regsvr32.exe /u /s SoundBoosterApo.dll

echo.
echo ========================================================
echo SonicBoost APO uninstalled and audio endpoints restored.
echo ========================================================
pause
