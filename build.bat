@echo off
setlocal
echo ========================================================
echo Building SonicBoost - High-Performance Audio Engine
echo ========================================================

call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"

cd /d "%~dp0"

echo.
echo [1/2] Building SoundBoosterApo.dll...
cd apo
call build_apo.bat
cd ..

echo.
echo [2/2] Building SonicBoost.exe...
rc.exe resource.rc

cl.exe /O2 /EHsc /std:c++17 /DUNICODE /D_UNICODE /W3 ^
    main.cpp ^
    SettingsManager.cpp ^
    DspEngine.cpp ^
    ApoController.cpp ^
    ApoManager.cpp ^
    HookController.cpp ^
    AudioEngine.cpp ^
    SliderPopup.cpp ^
    TrayManager.cpp ^
    SettingsDialog.cpp ^
    resource.res ^
    /Fe:SonicBoost.exe ^
    /link /SUBSYSTEM:WINDOWS ^
    user32.lib gdi32.lib comctl32.lib shell32.lib advapi32.lib ole32.lib shlwapi.lib

if %ERRORLEVEL% EQU 0 (
    echo.
    echo ========================================================
    echo SonicBoost Build Succeeded!
    echo Binary: SonicBoost.exe
    echo ========================================================
) else (
    echo.
    echo [ERROR] Build Failed with exit code %ERRORLEVEL%
)
