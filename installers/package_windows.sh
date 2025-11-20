#!/bin/bash
# Delta CLI - Windows Installer Builder
# Creates a Windows .exe installer using NSIS or portable ZIP
# Can be run on macOS/Linux using Wine + NSIS, or on Windows directly

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

error_exit() {
    echo -e "${RED}❌ Error: $1${NC}" >&2
    exit 1
}

info() {
    echo -e "${BLUE}ℹ $1${NC}"
}

success() {
    echo -e "${GREEN}✓ $1${NC}"
}

warning() {
    echo -e "${YELLOW}⚠ $1${NC}"
}

# Get script directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
cd "$PROJECT_DIR"

# Configuration
APP_NAME="Delta CLI"
APP_ID="DeltaCLI"
VERSION="${VERSION:-1.0.0}"
BUILD_DIR="${BUILD_DIR:-build_windows/Release}"
PACKAGE_DIR="$SCRIPT_DIR/packages"
INSTALLER_NAME="${APP_ID}-Setup-${VERSION}.exe"

# Detect OS
if [[ "$OSTYPE" == "msys" || "$OSTYPE" == "win32" || "$OSTYPE" == "cygwin" ]]; then
    IS_WINDOWS=true
    BUILD_DIR_WIN="$BUILD_DIR"
else
    IS_WINDOWS=false
    # Convert paths for cross-platform use
    BUILD_DIR_WIN=$(echo "$BUILD_DIR" | sed 's|/|\\|g')
fi

echo "╔══════════════════════════════════════════════════════════════╗"
echo "║         Delta CLI - Windows Installer Builder                ║"
echo "╚══════════════════════════════════════════════════════════════╝"
echo ""

# Step 1: Verify build exists
info "Step 1/5: Verifying build..."
if [ ! -f "$BUILD_DIR/delta.exe" ]; then
    # Try alternative locations
    ALTERNATIVE_BUILDS=("build_windows/Release" "build_windows/Debug" "build/Release" "build/Debug")
    BUILD_FOUND=false
    
    for alt_build in "${ALTERNATIVE_BUILDS[@]}"; do
        if [ -f "$alt_build/delta.exe" ]; then
            warning "Build not found at $BUILD_DIR/delta.exe, but found at $alt_build/delta.exe"
            info "Using $alt_build instead"
            BUILD_DIR="$alt_build"
            BUILD_DIR_WIN=$(echo "$BUILD_DIR" | sed 's|/|\\|g')
            BUILD_FOUND=true
            break
        fi
    done
    
    if [ "$BUILD_FOUND" = false ]; then
        error_exit "Build not found at $BUILD_DIR/delta.exe. Please run build_windows.bat first."
    fi
fi
success "Build found at $BUILD_DIR/delta.exe"

# Step 2: Create package directory
info "Step 2/5: Creating package structure..."
mkdir -p "$PACKAGE_DIR"

# Step 3: Create NSIS installer script
info "Step 3/5: Creating NSIS installer script..."

NSIS_SCRIPT="$SCRIPT_DIR/delta_setup.nsi"

cat > "$NSIS_SCRIPT" <<EOF
; Delta CLI Windows Installer Script
; Generated automatically - do not edit manually

!include "MUI2.nsh"

; Product Information
!define PRODUCT_NAME "${APP_NAME}"
!define PRODUCT_VERSION "${VERSION}"
!define PRODUCT_PUBLISHER "Delta CLI Team"
!define PRODUCT_WEB_SITE "https://github.com/oderoi/delta-cli"
!define PRODUCT_UNINST_KEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APP_ID}"
!define PRODUCT_UNINST_ROOT_KEY "HKLM"

; Installer Settings
Name "\${PRODUCT_NAME} \${PRODUCT_VERSION}"
OutFile "${PACKAGE_DIR}/${INSTALLER_NAME}"
InstallDir "\$PROGRAMFILES64\\${APP_NAME}"
RequestExecutionLevel admin
SetCompressor /SOLID lzma

; Modern UI
!define MUI_ICON "\${NSISDIR}\\Contrib\\Graphics\\Icons\\modern-install.ico"
!define MUI_UNICON "\${NSISDIR}\\Contrib\\Graphics\\Icons\\modern-uninstall.ico"
!define MUI_HEADERIMAGE
!define MUI_HEADERIMAGE_BITMAP "\${NSISDIR}\\Contrib\\Graphics\\Header\\win.bmp"
!define MUI_WELCOMEFINISHPAGE_BITMAP "\${NSISDIR}\\Contrib\\Graphics\\Wizard\\win.bmp"
!define MUI_UNWELCOMEFINISHPAGE_BITMAP "\${NSISDIR}\\Contrib\\Graphics\\Wizard\\win.bmp"

; Pages
!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_LICENSE "\${TEMPDIR}\\LICENSE.txt"
!insertmacro MUI_PAGE_COMPONENTS
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_WELCOME
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_UNPAGE_FINISH

; Languages
!insertmacro MUI_LANGUAGE "English"

; Installer Sections
Section "!${APP_NAME}" SEC01
    SectionIn RO
    
    SetOutPath "\$INSTDIR"
    
    ; Copy main executable
    File "${BUILD_DIR_WIN}\\delta.exe"
    
    ; Copy server executable if available
    IfFileExists "${BUILD_DIR_WIN}\\delta-server.exe" 0 +2
    File "${BUILD_DIR_WIN}\\delta-server.exe"
    
    ; Copy web UI if available (will be handled by script, not NSIS)
    ; Web UI should be copied manually before building installer
    
    ; Create Start Menu shortcuts
    CreateDirectory "\$SMPROGRAMS\\${APP_NAME}"
    CreateShortCut "\$SMPROGRAMS\\${APP_NAME}\\${APP_NAME}.lnk" "\$INSTDIR\\delta.exe"
    CreateShortCut "\$SMPROGRAMS\\${APP_NAME}\\Uninstall.lnk" "\$INSTDIR\\uninstall.exe"
    
    ; Add to PATH (if EnVar plugin is available)
    ; EnVar::SetHKLM
    ; EnVar::AddValue "Path" "\$INSTDIR"
    ; Pop \$0
    
    ; Write registry for Add/Remove Programs
    WriteRegStr \${PRODUCT_UNINST_ROOT_KEY} "\${PRODUCT_UNINST_KEY}" "DisplayName" "\$(^Name)"
    WriteRegStr \${PRODUCT_UNINST_ROOT_KEY} "\${PRODUCT_UNINST_KEY}" "UninstallString" "\$INSTDIR\\uninstall.exe"
    WriteRegStr \${PRODUCT_UNINST_ROOT_KEY} "\${PRODUCT_UNINST_KEY}" "DisplayIcon" "\$INSTDIR\\delta.exe"
    WriteRegStr \${PRODUCT_UNINST_ROOT_KEY} "\${PRODUCT_UNINST_KEY}" "DisplayVersion" "\${PRODUCT_VERSION}"
    WriteRegStr \${PRODUCT_UNINST_ROOT_KEY} "\${PRODUCT_UNINST_KEY}" "URLInfoAbout" "\${PRODUCT_WEB_SITE}"
    WriteRegStr \${PRODUCT_UNINST_ROOT_KEY} "\${PRODUCT_UNINST_KEY}" "Publisher" "\${PRODUCT_PUBLISHER}"
    
    ; Create uninstaller
    WriteUninstaller "\$INSTDIR\\uninstall.exe"
SectionEnd

Section "Add to PATH" SEC02
    ; Add to system PATH (requires system restart to take effect)
    ReadRegStr \$0 HKLM "SYSTEM\CurrentControlSet\Control\Session Manager\Environment" "Path"
    StrCmp \$0 "" AddToPath
    StrCpy \$1 "\$0"
    StrCpy \$2 "\$INSTDIR"
    Call AddToPathFunction
    Goto Done
    AddToPath:
    WriteRegExpandStr HKLM "SYSTEM\CurrentControlSet\Control\Session Manager\Environment" "Path" "\$INSTDIR"
    Done:
SectionEnd

Function AddToPathFunction
    Push \$3
    Push \$4
    Push \$5
    ReadRegStr \$3 HKLM "SYSTEM\CurrentControlSet\Control\Session Manager\Environment" "Path"
    StrCpy \$5 \$3 1 -1
    StrCmp \$5 ";" +2
    StrCpy \$3 "\$3;"
    Push "\$3"
    Push "\$2;"
    Call StrStr
    Pop \$4
    StrCmp \$4 "" AddToPath_NF
    Goto AddToPath_Done
    AddToPath_NF:
    StrCpy \$3 "\$3\$2"
    WriteRegExpandStr HKLM "SYSTEM\CurrentControlSet\Control\Session Manager\Environment" "Path" \$3
    AddToPath_Done:
    Pop \$5
    Pop \$4
    Pop \$3
FunctionEnd

Function StrStr
    Exch \$R1
    Exch
    Exch \$R2
    Push \$R3
    Push \$R4
    Push \$R5
    StrLen \$R3 \$R1
    StrCpy \$R4 0
    loop:
        StrCpy \$R5 \$R1 \$R3 \$R4
        StrCmp \$R5 \$R2 done
        StrCmp \$R5 "" done
        IntOp \$R4 \$R4 + 1
        Goto loop
    done:
        StrCpy \$R1 \$R4
        Pop \$R5
        Pop \$R4
        Pop \$R3
        Pop \$R2
        Exch \$R1
FunctionEnd

Section "Desktop Shortcut" SEC03
    CreateShortCut "\$DESKTOP\\${APP_NAME}.lnk" "\$INSTDIR\\delta.exe"
SectionEnd

; Descriptions
LangString DESC_SEC01 \${LANG_ENGLISH} "Install ${APP_NAME} and required files."
LangString DESC_SEC02 \${LANG_ENGLISH} "Add ${APP_NAME} to system PATH for command-line access."
LangString DESC_SEC03 \${LANG_ENGLISH} "Create a desktop shortcut for ${APP_NAME}."

!insertmacro MUI_FUNCTION_DESCRIPTION_BEGIN
  !insertmacro MUI_DESCRIPTION_TEXT \${SEC01} \$(DESC_SEC01)
  !insertmacro MUI_DESCRIPTION_TEXT \${SEC02} \$(DESC_SEC02)
  !insertmacro MUI_DESCRIPTION_TEXT \${SEC03} \$(DESC_SEC03)
!insertmacro MUI_FUNCTION_DESCRIPTION_END

; Uninstaller Section
Section "Uninstall"
    ; Remove files
    Delete "\$INSTDIR\\delta.exe"
    Delete "\$INSTDIR\\delta-server.exe"
    Delete "\$INSTDIR\\delta-logo.png"
    RMDir /r "\$INSTDIR\\webui"
    Delete "\$INSTDIR\\uninstall.exe"
    
    ; Remove shortcuts
    Delete "\$SMPROGRAMS\\${APP_NAME}\\${APP_NAME}.lnk"
    Delete "\$SMPROGRAMS\\${APP_NAME}\\Uninstall.lnk"
    RMDir "\$SMPROGRAMS\\${APP_NAME}"
    Delete "\$DESKTOP\\${APP_NAME}.lnk"
    
    ; Remove from PATH
    ReadRegStr \$0 HKLM "SYSTEM\CurrentControlSet\Control\Session Manager\Environment" "Path"
    Push "\$0"
    Push "\$INSTDIR"
    Call un.RemoveFromPath
    Pop \$0
    WriteRegExpandStr HKLM "SYSTEM\CurrentControlSet\Control\Session Manager\Environment" "Path" \$0
    
    ; Remove registry entries
    DeleteRegKey \${PRODUCT_UNINST_ROOT_KEY} "\${PRODUCT_UNINST_KEY}"
    
    ; Remove installation directory
    RMDir "\$INSTDIR"
SectionEnd

; Helper function to remove from PATH
Function un.RemoveFromPath
    Exch \$0
    Exch
    Exch \$1
    Push \$2
    Push \$3
    Push \$4
    Push \$5
    Push \$6
    
    ReadRegStr \$2 HKLM "SYSTEM\CurrentControlSet\Control\Session Manager\Environment" "Path"
    StrCpy \$5 \$2 1 -1
    StrCmp \$5 ";" +2
    StrCpy \$2 "\$2;"
    Push "\$2"
    Push "\$1;"
    Call un.StrStr
    Pop \$3
    StrCmp \$3 "" unRemoveFromPath_done
    StrLen \$4 "\$1;"
    StrLen \$5 \$3
    StrCpy \$6 \$2 -\$5
    StrCpy \$6 \$6 "" \$4
    StrCpy \$5 \$6 1 -1
    StrCmp \$5 ";" 0 +2
    StrCpy \$6 \$6 -1
    StrCpy \$0 \$6
    
    unRemoveFromPath_done:
    Pop \$6
    Pop \$5
    Pop \$4
    Pop \$3
    Pop \$2
    Pop \$1
    Exch \$0
FunctionEnd

Function un.StrStr
    Exch \$R1
    Exch
    Exch \$R2
    Push \$R3
    Push \$R4
    Push \$R5
    StrLen \$R3 \$R1
    StrCpy \$R4 0
    loop:
        StrCpy \$R5 \$R1 \$R3 \$R4
        StrCmp \$R5 \$R2 done
        StrCmp \$R5 "" done
        IntOp \$R4 \$R4 + 1
        Goto loop
    done:
        StrCpy \$R1 \$R4
        Pop \$R5
        Pop \$R4
        Pop \$R3
        Pop \$R2
        Exch \$R1
FunctionEnd
EOF

# Copy LICENSE for NSIS
if [ -f "LICENSE" ]; then
    cp "LICENSE" "/tmp/LICENSE.txt" 2>/dev/null || cp "LICENSE" "${TMPDIR}/LICENSE.txt" 2>/dev/null || true
fi

success "NSIS script created: $NSIS_SCRIPT"

# Step 4: Build installer
info "Step 4/5: Building installer..."

# Check for NSIS
MAKENSIS=""
if command -v makensis >/dev/null 2>&1; then
    MAKENSIS="makensis"
elif [ -f "/usr/bin/makensis" ]; then
    MAKENSIS="/usr/bin/makensis"
elif [ -f "/usr/local/bin/makensis" ]; then
    MAKENSIS="/usr/local/bin/makensis"
elif [ -f "C:/Program Files (x86)/NSIS/makensis.exe" ]; then
    MAKENSIS="C:/Program Files (x86)/NSIS/makensis.exe"
elif [ -f "C:/Program Files/NSIS/makensis.exe" ]; then
    MAKENSIS="C:/Program Files/NSIS/makensis.exe"
fi

if [ -n "$MAKENSIS" ]; then
    info "Found NSIS: $MAKENSIS"
    
    # Try to build with NSIS
    if $MAKENSIS "$NSIS_SCRIPT" >/dev/null 2>&1; then
        if [ -f "$PACKAGE_DIR/$INSTALLER_NAME" ]; then
            success "Installer created: $PACKAGE_DIR/$INSTALLER_NAME"
            
            # Calculate checksum
            if command -v sha256sum >/dev/null 2>&1; then
                SHA256=$(sha256sum "$PACKAGE_DIR/$INSTALLER_NAME" | cut -d' ' -f1)
            elif command -v shasum >/dev/null 2>&1; then
                SHA256=$(shasum -a 256 "$PACKAGE_DIR/$INSTALLER_NAME" | cut -d' ' -f1)
            fi
            
            if [ -n "$SHA256" ]; then
                echo "$SHA256  $(basename "$PACKAGE_DIR/$INSTALLER_NAME")" > "$PACKAGE_DIR/${INSTALLER_NAME}.sha256"
                info "SHA256: $SHA256"
            fi
        else
            warning "NSIS build completed but installer not found. Creating portable ZIP instead..."
            MAKENSIS=""
        fi
    else
        warning "NSIS build failed. Creating portable ZIP instead..."
        MAKENSIS=""
    fi
fi

# Fallback: Create portable ZIP
if [ -z "$MAKENSIS" ] || [ ! -f "$PACKAGE_DIR/$INSTALLER_NAME" ]; then
    info "Creating portable ZIP package (NSIS not available)..."
    
    PORTABLE_DIR="$PACKAGE_DIR/${APP_ID}-${VERSION}-portable"
    rm -rf "$PORTABLE_DIR"
    mkdir -p "$PORTABLE_DIR"
    
    # Copy binaries
    cp "$BUILD_DIR/delta.exe" "$PORTABLE_DIR/"
    if [ -f "$BUILD_DIR/delta-server.exe" ]; then
        cp "$BUILD_DIR/delta-server.exe" "$PORTABLE_DIR/"
    fi
    
    # Copy web UI
    if [ -d "public" ] && ([ -f "public/index.html" ] || [ -f "public/index.html.gz" ]); then
        cp -r public "$PORTABLE_DIR/webui"
    fi
    
    # Copy logo
    if [ -f "assets/delta-logo.png" ]; then
        cp "assets/delta-logo.png" "$PORTABLE_DIR/"
    fi
    
    # Create README
    cat > "$PORTABLE_DIR/README.txt" <<README_EOF
Delta CLI ${VERSION} - Portable Edition
========================================

This is a portable version of Delta CLI. No installation required!

USAGE:
------
1. Extract this ZIP to any location
2. Run delta.exe from the extracted folder
3. Or add the folder to your PATH for command-line access

QUICK START:
------------
delta.exe --version          # Check version
delta.exe pull qwen3:0.6b    # Download a model
delta.exe                    # Start chatting

For more information, visit:
https://github.com/oderoi/delta-cli
README_EOF
    
    # Create ZIP
    PORTABLE_ZIP="$PACKAGE_DIR/${APP_ID}-${VERSION}-portable.zip"
    if command -v zip >/dev/null 2>&1; then
        cd "$PACKAGE_DIR"
        zip -r "${APP_ID}-${VERSION}-portable.zip" "${APP_ID}-${VERSION}-portable" >/dev/null 2>&1
        cd "$PROJECT_DIR"
        rm -rf "$PORTABLE_DIR"
        success "Portable ZIP created: $PORTABLE_ZIP"
        
        # Calculate checksum
        if command -v sha256sum >/dev/null 2>&1; then
            SHA256=$(sha256sum "$PORTABLE_ZIP" | cut -d' ' -f1)
        elif command -v shasum >/dev/null 2>&1; then
            SHA256=$(shasum -a 256 "$PORTABLE_ZIP" | cut -d' ' -f1)
        fi
        
        if [ -n "$SHA256" ]; then
            echo "$SHA256  $(basename "$PORTABLE_ZIP")" > "${PORTABLE_ZIP}.sha256"
            info "SHA256: $SHA256"
        fi
        
        warning "Note: To create a proper .exe installer, install NSIS:"
        warning "  Windows: Download from https://nsis.sourceforge.io/Download"
        warning "  macOS/Linux: Use Wine + NSIS or build on Windows"
    else
        error_exit "zip command not found. Install zip utility."
    fi
fi

# Step 5: Summary
info "Step 5/5: Summary"
echo ""
if [ -f "$PACKAGE_DIR/$INSTALLER_NAME" ]; then
    success "✅ Windows installer created successfully!"
    echo ""
    info "Installer: $PACKAGE_DIR/$INSTALLER_NAME"
    echo ""
    info "To distribute:"
    echo "  - Upload to GitHub Releases"
    echo "  - Or host on your website"
    echo ""
    info "Users can install by:"
    echo "  1. Downloading the installer"
    echo "  2. Double-clicking to run"
    echo "  3. Following the installation wizard"
    echo ""
elif [ -f "$PORTABLE_ZIP" ]; then
    success "✅ Portable ZIP package created!"
    echo ""
    info "Package: $PORTABLE_ZIP"
    echo ""
    info "Users can:"
    echo "  1. Download and extract the ZIP"
    echo "  2. Run delta.exe from the extracted folder"
    echo ""
fi

success "Done!"

