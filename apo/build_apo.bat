@echo off
setlocal
echo ========================================================
echo Building SoundBooster APO DLL (64-bit)
echo ========================================================

call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"

cd /d "%~dp0"

cl.exe /O2 /EHsc /std:c++17 /DUNICODE /D_UNICODE /LD /W3 ^
    SoundBoosterApo.cpp ^
    dllmain.cpp ^
    /Fe:SoundBoosterApo.dll ^
    /link /DEF:SoundBoosterApo.def ^
    audiobaseprocessingobject.lib audiomediatypecrt.lib audioeng.lib advapi32.lib ole32.lib user32.lib

if %ERRORLEVEL% EQU 0 (
    echo.
    echo ========================================================
    echo SoundBoosterApo.dll Built Successfully!
    echo ========================================================
    copy /Y SoundBoosterApo.dll ..\SoundBoosterApo.dll
) else (
    echo.
    echo [ERROR] APO Build Failed with code %ERRORLEVEL%
)
