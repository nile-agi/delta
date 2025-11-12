@echo off
REM Build script for Windows (x64)

echo ╔══════════════════════════════════════════════════════════════╗
echo ║         Delta CLI - Windows Build Script                    ║
echo ╚══════════════════════════════════════════════════════════════╝

REM Configuration
set BUILD_TYPE=Release
set BUILD_DIR=build_windows
set ENABLE_CUDA=OFF

REM Check for CUDA
where nvcc >nul 2>&1
if %ERRORLEVEL% EQU 0 (
    echo CUDA detected - GPU acceleration will be available
    set ENABLE_CUDA=ON
) else (
    echo CUDA not found - building CPU-only version
)

REM Check dependencies
where cmake >nul 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo Error: cmake is required but not installed.
    echo Install from: https://cmake.org/download/
    exit /b 1
)

where cl >nul 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo Error: MSVC compiler not found.
    echo Run this script from Visual Studio Developer Command Prompt
    exit /b 1
)

REM Check for vendored llama-cpp
if not exist "vendor\llama-cpp\CMakeLists.txt" (
    echo Error: llama-cpp not found in vendor\llama-cpp\
    echo Please ensure the vendored copy is present.
    exit /b 1
)
echo Vendored llama-cpp found

REM Create build directory
if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"
cd "%BUILD_DIR%"

REM Configure
echo Configuring build...
cmake .. ^
    -DCMAKE_BUILD_TYPE=%BUILD_TYPE% ^
    -DGGML_CUDA=%ENABLE_CUDA% ^
    -DUSE_CURL=ON ^
    -DBUILD_TESTS=ON ^
    -DBUILD_SERVER=ON

if %ERRORLEVEL% NEQ 0 (
    echo Configuration failed!
    exit /b 1
)

REM Build
echo Building...
cmake --build . --config %BUILD_TYPE% -j

if %ERRORLEVEL% NEQ 0 (
    echo Build failed!
    exit /b 1
)

REM Test
echo Running tests...
ctest --output-on-failure -C %BUILD_TYPE%

echo.
echo Build complete!
echo Binary location: %BUILD_DIR%\%BUILD_TYPE%\delta.exe
echo.
echo To create installer:
echo   installers\package_windows.bat
echo.
echo To run:
echo   %BUILD_DIR%\%BUILD_TYPE%\delta.exe --help

cd ..

