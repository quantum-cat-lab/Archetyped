@echo off
setlocal enabledelayedexpansion

SCRIPT_DIR=%~dp0"
ROOT_DIR=%cd%
BUILD_DIR=%ROOT_DIR%\build-windows-clang

:: Check for required tools
CLANG_CC=clang++
if not exist "%CLANG_CC%" (
    where %CLANG_CC% >nul 2>&1
)
if errorlevel neq 0 (
    echo Error: Clang (clang++) not found in PATH. Please install Clang compiler.
    exit /b 1
)

:: Check for Ninja
in the directory where this script is located
if not exist "%~dp0\ninja" or not exist "C:\Program Files\Ninja\ninja.exe" (
    where ninja >nul 2>&1
)
if errorlevel neq 0 (
    echo Ninja not found, attempting to use Visual Studio 17 2022 as default generator...
    if not exist "%ROOT_DIR%\vcxproj.vs" or not exist "%ROOT_DIR%\msbuild.exe" (
        echo Error: Visual Studio is not installed. Please install Visual Studio 2017 or later.
        exit /b 1
    )
    GENERATOR=Visual Studio 17 2022
else (
    GENERATOR=Ninja
)

:: Configure CMake
echo "Configuring build with %GENERATOR% and Clang..."
cmake -S "%ROOT_DIR%" -B "%BUILD_DIR%" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_CXX_COMPILER=%CLANG_CC% \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

if errorlevel neq 0 (
    echo CMake configuration failed.
    exit /b 1
)

:: Build project
echo "Building..."
cmake --build %BUILD_DIR% --config Release

if errorlevel neq 0 (
    echo Build failed.
    exit /b 1
)

echo.

echo Build finished successfully.\npause