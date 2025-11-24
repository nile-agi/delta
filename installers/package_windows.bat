@echo off
REM Create Windows installer using NSIS (Nullsoft Scriptable Install System)

echo Creating Windows installer...

set VERSION=1.0.0
set BUILD_DIR=build_windows\Release
set INSTALLER_DIR=installers\packages

REM Check if binary exists
if not exist "%BUILD_DIR%\delta.exe" (
    echo Error: Binary not found. Run build_windows.bat first.
    exit /b 1
)

REM Create installer directory
if not exist "%INSTALLER_DIR%" mkdir "%INSTALLER_DIR%"

REM Create NSIS script
echo Creating NSIS script...
(
echo !define PRODUCT_NAME "Delta CLI"
echo !define PRODUCT_VERSION "%VERSION%"
echo !define PRODUCT_PUBLISHER "Delta CLI Team"
echo !define PRODUCT_WEB_SITE "https://github.com/yourusername/delta-cli"
echo.
echo SetCompressor lzma
echo Name "${PRODUCT_NAME} ${PRODUCT_VERSION}"
echo OutFile "%INSTALLER_DIR%\DeltaCLI-Setup-${PRODUCT_VERSION}.exe"
echo InstallDir "$PROGRAMFILES64\Delta CLI"
echo.
echo Page directory
echo Page instfiles
echo.
echo Section "MainSection" SEC01
echo   SetOutPath "$INSTDIR"
echo   File "%BUILD_DIR%\delta.exe"
echo   CreateDirectory "$SMPROGRAMS\Delta CLI"
echo   CreateShortCut "$SMPROGRAMS\Delta CLI\Delta CLI.lnk" "$INSTDIR\delta.exe"
echo   CreateShortCut "$DESKTOP\Delta CLI.lnk" "$INSTDIR\delta.exe"
echo   WriteUninstaller "$INSTDIR\uninst.exe"
echo SectionEnd
echo.
echo Section "Uninstall"
echo   Delete "$INSTDIR\delta.exe"
echo   Delete "$INSTDIR\uninst.exe"
echo   Delete "$SMPROGRAMS\Delta CLI\Delta CLI.lnk"
echo   Delete "$DESKTOP\Delta CLI.lnk"
echo   RMDir "$SMPROGRAMS\Delta CLI"
echo   RMDir "$INSTDIR"
echo SectionEnd
) > installers\delta_setup.nsi

REM Check for NSIS
where makensis >nul 2>&1
if %ERRORLEVEL% EQU 0 (
    echo Building installer with NSIS...
    makensis installers\delta_setup.nsi
    echo.
    echo Installer created: %INSTALLER_DIR%\DeltaCLI-Setup-%VERSION%.exe
) else (
    echo NSIS not found. Creating portable ZIP instead...
    
    REM Create portable package
    powershell -Command "Compress-Archive -Path '%BUILD_DIR%\delta.exe' -DestinationPath '%INSTALLER_DIR%\DeltaCLI-%VERSION%-portable.zip' -Force"
    
    echo.
    echo Portable package created: %INSTALLER_DIR%\DeltaCLI-%VERSION%-portable.zip
    echo.
    echo To create a proper installer, install NSIS from:
    echo https://nsis.sourceforge.io/Download
)

