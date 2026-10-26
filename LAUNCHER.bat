@echo off
cd /d "%~dp0build\Release"
if not exist "SecretOfEvermoreSNESRecomp.exe" (
    echo No se encuentra SecretOfEvermoreSNESRecomp.exe. Ejecuta primero COMPILAR.bat.
    pause
    exit /b 1
)
start "" "SecretOfEvermoreSNESRecomp.exe" --launcher
exit /b
