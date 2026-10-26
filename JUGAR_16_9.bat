@echo off
cd /d "%~dp0build\Release"
if not exist "SecretOfEvermoreSNESRecomp.exe" (
    echo No se encuentra SecretOfEvermoreSNESRecomp.exe. Ejecuta primero COMPILAR.bat.
    pause
    exit /b 1
)
copy /y config_16_9.ini config.ini >nul 2>&1
start "" "SecretOfEvermoreSNESRecomp.exe" --config config_16_9.ini --no-launcher
exit /b
