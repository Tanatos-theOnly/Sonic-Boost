@echo off
setlocal
echo ========================================================
echo Installing SonicBoost Audio Processing Object (APO)
echo ========================================================
cd /d "%~dp0"

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
