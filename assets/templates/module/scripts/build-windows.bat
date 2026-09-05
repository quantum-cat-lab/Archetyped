@echo off
setlocal enabledelayedexpansion

set "SCRIPT_DIR=%~dp0"
cd /d "%SCRIPT_DIR%.."
set "ROOT_DIR=%cd%"
set "BUILD_DIR=%ROOT_DIR%\build-windows-clang"

:: Проверяем наличие Clang
set "CLANG_CC=clang++"
where %CLANG_CC% >nul 2>nul
if %ERRORLEVEL% neq 0 (
    echo Error: Clang (clang++^) not found in PATH.
    exit /b 2
)

:: Пытаемся найти Ninja, если его нет — используем стандартный генератор
set "GENERATOR=Ninja"
where ninja >nul 2>nul
if %ERRORLEVEL% neq 0 (
    echo Ninja not found, falling back to default generator...
    set "GENERATOR=Visual Studio 17 2022"
)

echo Configuring build with %GENERATOR% and Clang...

:: Если используем Ninja, явно указываем компилятор. 
:: Если Visual Studio - используем флаг -T ClangCL
if "%GENERATOR%"=="Ninja" (
    cmake -S "%ROOT_DIR%" -B "%BUILD_DIR%" ^
          -G "Ninja" ^
          -DCMAKE_BUILD_TYPE=Release ^
          -DCMAKE_CXX_COMPILER=clang++ ^
          -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
) else (
    cmake -S "%ROOT_DIR%" -B "%BUILD_DIR%" ^
          -G "%GENERATOR%" ^
          -T ClangCL ^
          -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
)

if %ERRORLEVEL% neq 0 (
    echo CMake configuration failed.
    exit /b 1
)

:: Копируем compile_commands.json для работы clangd
if exist "%BUILD_DIR%\compile_commands.json" (
    copy /Y "%BUILD_DIR%\compile_commands.json" "%ROOT_DIR%\" >nul
)

echo Building...
cmake --build "%BUILD_DIR%" --config Release --parallel %NUMBER_OF_PROCESSORS%

if %ERRORLEVEL% neq 0 (
    echo Build failed.
    exit /b 1
)

echo.
echo Build finished successfully.
pause