@echo off
setlocal
echo ========================================================
echo Cleaning SonicBoost Build Artifacts
echo ========================================================

cd /d "%~dp0"

echo Removing intermediate compiler and linker objects...
del /q /f *.obj 2>nul
del /q /f *.res 2>nul
del /q /f *.exp 2>nul
del /q /f *.lib 2>nul
del /q /f *.ilk 2>nul
del /q /f *.pdb 2>nul

echo Cleaning apo directory...
cd apo
del /q /f *.obj 2>nul
del /q /f *.res 2>nul
del /q /f *.exp 2>nul
del /q /f *.lib 2>nul
del /q /f *.ilk 2>nul
del /q /f *.pdb 2>nul
cd ..

echo.
echo ========================================================
echo Clean Completed Successfully!
echo ========================================================
