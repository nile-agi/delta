@echo off
REM Create Windows ZIP package for winget installation
REM This script creates delta-cli-windows-x64.zip with the correct structure

echo ╔══════════════════════════════════════════════════════════════╗
echo ║    Delta CLI - Create Winget ZIP Package                    ║
echo ╚══════════════════════════════════════════════════════════════╝

set VERSION=1.0.0
set BUILD_DIR=build_windows\Release
set OUTPUT_DIR=installers\packages
set ZIP_NAME=delta-cli-windows-x64.zip
set TEMP_DIR=%TEMP%\delta-cli-package-%RANDOM%

REM Check if binaries exist
if not exist "%BUILD_DIR%\delta.exe" (
    echo Error: delta.exe not found in %BUILD_DIR%
    echo Please build the project first using: installers\build_windows.bat
    exit /b 1
)

if not exist "%BUILD_DIR%\delta-server.exe" (
    echo Error: delta-server.exe not found in %BUILD_DIR%
    echo Please ensure BUILD_SERVER=ON when building
    exit /b 1
)

echo.
echo Building package...
echo   Version: %VERSION%
echo   Build directory: %BUILD_DIR%
echo   Output: %OUTPUT_DIR%\%ZIP_NAME%
echo.

REM Create output directory
if not exist "%OUTPUT_DIR%" mkdir "%OUTPUT_DIR%"

REM Create temporary directory for packaging
if exist "%TEMP_DIR%" rmdir /s /q "%TEMP_DIR%"
mkdir "%TEMP_DIR%"

REM Copy executables to temp directory (at root level for winget)
echo Copying executables...
copy "%BUILD_DIR%\delta.exe" "%TEMP_DIR%\delta.exe" >nul
if %ERRORLEVEL% NEQ 0 (
    echo Error: Failed to copy delta.exe
    exit /b 1
)

copy "%BUILD_DIR%\delta-server.exe" "%TEMP_DIR%\delta-server.exe" >nul
if %ERRORLEVEL% NEQ 0 (
    echo Error: Failed to copy delta-server.exe
    exit /b 1
)

REM Check for required DLLs and copy them
echo Checking for required DLLs...
set DLLS_FOUND=0

REM Check for Visual C++ Runtime DLLs (common dependencies)
if exist "%BUILD_DIR%\*.dll" (
    echo Copying DLL files...
    copy "%BUILD_DIR%\*.dll" "%TEMP_DIR%\" >nul
    set DLLS_FOUND=1
)

REM Check for llama.cpp server executable (if built separately)
if exist "%BUILD_DIR%\server.exe" (
    echo Copying server.exe...
    copy "%BUILD_DIR%\server.exe" "%TEMP_DIR%\server.exe" >nul
)

REM Check for llama-server.exe (alternative name)
if exist "%BUILD_DIR%\llama-server.exe" (
    echo Copying llama-server.exe...
    copy "%BUILD_DIR%\llama-server.exe" "%TEMP_DIR%\llama-server.exe" >nul
)

REM Create ZIP file using PowerShell (more reliable than batch)
echo.
echo Creating ZIP archive...
powershell -NoProfile -ExecutionPolicy Bypass -Command ^
    "$ErrorActionPreference = 'Stop';" ^
    "$zipPath = Join-Path (Resolve-Path '%OUTPUT_DIR%') '%ZIP_NAME%';" ^
    "$tempPath = '%TEMP_DIR%';" ^
    "if (Test-Path $zipPath) { Remove-Item $zipPath -Force };" ^
    "Compress-Archive -Path (Join-Path $tempPath '*') -DestinationPath $zipPath -CompressionLevel Optimal -Force;" ^
    "Write-Host 'ZIP created successfully: ' $zipPath"

if %ERRORLEVEL% NEQ 0 (
    echo Error: Failed to create ZIP file
    rmdir /s /q "%TEMP_DIR%"
    exit /b 1
)

REM Clean up temp directory
rmdir /s /q "%TEMP_DIR%"

REM Display package info
echo.
echo ═══════════════════════════════════════════════════════════════
echo Package created successfully!
echo.
echo File: %OUTPUT_DIR%\%ZIP_NAME%
echo.

REM Calculate file size
for %%A in ("%OUTPUT_DIR%\%ZIP_NAME%") do (
    set SIZE=%%~zA
    set /a SIZE_MB=!SIZE!/1048576
    echo Size: !SIZE_MB! MB
)

echo.
echo Package contents:
echo   - delta.exe (at root level)
echo   - delta-server.exe (at root level)
if %DLLS_FOUND% EQU 1 (
    echo   - Required DLL files
)
echo.
echo Next steps:
echo   1. Upload %ZIP_NAME% to GitHub release v%VERSION%
echo   2. Calculate SHA256 hash: installers\calculate-sha256.bat
echo   3. Update winget manifest with SHA256 hash
echo.
echo ═══════════════════════════════════════════════════════════════
