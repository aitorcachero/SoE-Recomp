@echo off
setlocal
cd /d "%~dp0"
echo ===================================================
echo   Compilando Secret of Evermore SNES Recomp...
echo ===================================================

where cmake >nul 2>&1
if %errorlevel% neq 0 (
    if exist "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" (
        set "PATH=C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin;%PATH%"
    ) else if exist "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" (
        set "PATH=C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin;%PATH%"
    )
)

if not exist build (
    mkdir build
)

cmake -B build -S .
if %errorlevel% neq 0 (
    echo Error al configurar el proyecto con CMake.
    pause
    exit /b %errorlevel%
)

cmake --build build --config Release
if %errorlevel% neq 0 (
    echo Error al compilar el proyecto.
    pause
    exit /b %errorlevel%
)

echo.
echo ===================================================
echo   Compilacion finalizada con exito!
echo ===================================================
pause
