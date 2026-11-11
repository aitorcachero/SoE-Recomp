@echo off
setlocal
cd /d "%~dp0"
if exist "build\Release\SecretOfEvermoreSNESRecomp.exe" (
    start "" "build\Release\SecretOfEvermoreSNESRecomp.exe" --launcher
) else if exist "SecretOfEvermoreSNESRecomp.exe" (
    start "" "SecretOfEvermoreSNESRecomp.exe" --launcher
) else (
    echo No se encontro SecretOfEvermoreSNESRecomp.exe. Ejecuta primero COMPILAR.bat.
    pause
)
exit /b 0
