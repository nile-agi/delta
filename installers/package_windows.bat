@echo off
REM Delta CLI - Windows Installer Builder (Batch Script)
REM Creates a Windows .exe installer using NSIS

setlocal enabledelayedexpansion

echo ╔══════════════════════════════════════════════════════════════╗
echo ║         Delta CLI - Windows Installer Builder                ║
echo ╚══════════════════════════════════════════════════════════════╝
echo.

REM Configuration
set VERSION=1.0.0
set BUILD_DIR=build_windows\Release
set INSTALLER_DIR=installers\packages
set APP_NAME=Delta CLI
set APP_ID=DeltaCLI

REM Check if binary exists
if not exist "%BUILD_DIR%\delta.exe" (
    echo ❌ Error: Binary not found at %BUILD_DIR%\delta.exe
    echo.
    echo Please run build_windows.bat first.
    exit /b 1
)

echo ✓ Build found at %BUILD_DIR%\delta.exe
echo.

REM Create installer directory
if not exist "%INSTALLER_DIR%" mkdir "%INSTALLER_DIR%"

REM Check for NSIS
where makensis >nul 2>&1
if %ERRORLEVEL% EQU 0 (
    echo Creating NSIS installer...
    echo.
    
    REM Create NSIS script
    call :CreateNSISScript
    
    REM Build installer
    makensis installers\delta_setup.nsi
    
    if exist "%INSTALLER_DIR%\%APP_ID%-Setup-%VERSION%.exe" (
        echo.
        echo ✅ Installer created successfully!
        echo    Location: %INSTALLER_DIR%\%APP_ID%-Setup-%VERSION%.exe
        echo.
    ) else (
        echo.
        echo ⚠ NSIS build completed but installer not found.
        echo   Creating portable ZIP instead...
        call :CreatePortableZIP
    )
) else (
    echo ⚠ NSIS not found. Creating portable ZIP instead...
    echo.
    echo To create a proper installer, install NSIS from:
    echo   https://nsis.sourceforge.io/Download
    echo.
    call :CreatePortableZIP
)

echo.
echo Done!
exit /b 0

:CreateNSISScript
echo Creating NSIS installer script...
(
echo ; Delta CLI Windows Installer Script
echo ; Auto-generated - do not edit manually
echo.
echo !include "MUI2.nsh"
echo.
echo !define PRODUCT_NAME "%APP_NAME%"
echo !define PRODUCT_VERSION "%VERSION%"
echo !define PRODUCT_PUBLISHER "Delta CLI Team"
echo !define PRODUCT_WEB_SITE "https://github.com/oderoi/delta-cli"
echo !define PRODUCT_UNINST_KEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\%APP_ID%"
echo !define PRODUCT_UNINST_ROOT_KEY "HKLM"
echo.
echo Name "${PRODUCT_NAME} ${PRODUCT_VERSION}"
echo OutFile "%INSTALLER_DIR%\%APP_ID%-Setup-${PRODUCT_VERSION}.exe"
echo InstallDir "$PROGRAMFILES64\%APP_NAME%"
echo RequestExecutionLevel admin
echo SetCompressor /SOLID lzma
echo.
echo !insertmacro MUI_PAGE_WELCOME
echo !insertmacro MUI_PAGE_DIRECTORY
echo !insertmacro MUI_PAGE_INSTFILES
echo !insertmacro MUI_PAGE_FINISH
echo !insertmacro MUI_UNPAGE_CONFIRM
echo !insertmacro MUI_UNPAGE_INSTFILES
echo !insertmacro MUI_LANGUAGE "English"
echo.
echo Section "!Main" SEC01
echo     SetOutPath "$INSTDIR"
echo     File "%BUILD_DIR%\delta.exe"
echo     IfFileExists "%BUILD_DIR%\delta-server.exe" 0 +2
echo     File "%BUILD_DIR%\delta-server.exe"
echo     WriteUninstaller "$INSTDIR\uninstall.exe"
echo     CreateDirectory "$SMPROGRAMS\%APP_NAME%"
echo     CreateShortCut "$SMPROGRAMS\%APP_NAME%\%APP_NAME%.lnk" "$INSTDIR\delta.exe"
echo     CreateShortCut "$SMPROGRAMS\%APP_NAME%\Uninstall.lnk" "$INSTDIR\uninstall.exe"
echo     WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "DisplayName" "$(^Name)"
echo     WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "UninstallString" "$INSTDIR\uninstall.exe"
echo     WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "DisplayVersion" "${PRODUCT_VERSION}"
echo     WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "Publisher" "${PRODUCT_PUBLISHER}"
echo SectionEnd
echo.
echo Section "Desktop Shortcut"
echo     CreateShortCut "$DESKTOP\%APP_NAME%.lnk" "$INSTDIR\delta.exe"
echo SectionEnd
echo.
echo Section "Uninstall"
echo     Delete "$INSTDIR\delta.exe"
echo     Delete "$INSTDIR\delta-server.exe"
echo     Delete "$INSTDIR\uninstall.exe"
echo     Delete "$SMPROGRAMS\%APP_NAME%\%APP_NAME%.lnk"
echo     Delete "$SMPROGRAMS\%APP_NAME%\Uninstall.lnk"
echo     RMDir "$SMPROGRAMS\%APP_NAME%"
echo     Delete "$DESKTOP\%APP_NAME%.lnk"
echo     DeleteRegKey ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}"
echo     RMDir "$INSTDIR"
echo SectionEnd
) > installers\delta_setup.nsi
goto :eof

:CreatePortableZIP
echo Creating portable ZIP package...
powershell -Command "$ErrorActionPreference='Stop'; $ProgressPreference='SilentlyContinue'; Compress-Archive -Path '%BUILD_DIR%\delta.exe' -DestinationPath '%INSTALLER_DIR%\%APP_ID%-%VERSION%-portable.zip' -Force"
if exist "%INSTALLER_DIR%\%APP_ID%-%VERSION%-portable.zip" (
    echo ✅ Portable ZIP created: %INSTALLER_DIR%\%APP_ID%-%VERSION%-portable.zip
) else (
    echo ❌ Failed to create portable ZIP
)
goto :eof
