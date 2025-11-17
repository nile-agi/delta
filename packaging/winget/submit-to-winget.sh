#!/bin/bash
# Automated Winget Submission Script for Delta CLI (Bash version for cross-platform)
# This script prepares the winget submission but requires Windows for final steps

set -e

VERSION="${1:-1.0.0}"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
MANIFEST_PATH="$SCRIPT_DIR/delta-cli.yaml"
RELEASE_ZIP="$PROJECT_ROOT/release/delta-cli-windows-x64.zip"

echo "╔══════════════════════════════════════════════════════════════╗"
echo "║     Delta CLI - Winget Submission Preparation               ║"
echo "╚══════════════════════════════════════════════════════════════╝"
echo ""

# Check if release ZIP exists
if [ ! -f "$RELEASE_ZIP" ]; then
    echo "❌ Release package not found: $RELEASE_ZIP"
    echo ""
    echo "💡 To create the release package, run:"
    echo "   ./packaging/build-scripts/build-windows.ps1 Release x64"
    echo "   ./packaging/release/package-windows.ps1 $VERSION x64"
    echo ""
    exit 1
fi

echo "✅ Release package found: $RELEASE_ZIP"

# Calculate SHA256 (works on macOS/Linux)
if command -v shasum &> /dev/null; then
    SHA256=$(shasum -a 256 "$RELEASE_ZIP" | cut -d' ' -f1)
elif command -v sha256sum &> /dev/null; then
    SHA256=$(sha256sum "$RELEASE_ZIP" | cut -d' ' -f1)
else
    echo "❌ Cannot calculate SHA256. Install shasum or sha256sum."
    exit 1
fi

echo "✅ SHA256: $SHA256"
echo ""

# Update manifest
echo "📝 Updating manifest..."
if [ ! -f "$MANIFEST_PATH" ]; then
    echo "❌ Manifest not found: $MANIFEST_PATH"
    exit 1
fi

# Update version
sed -i.bak "s/PackageVersion: .*/PackageVersion: $VERSION/" "$MANIFEST_PATH"

# Update SHA256
sed -i.bak "s/InstallerSha256: .*/InstallerSha256: $SHA256/" "$MANIFEST_PATH"

# Update URL
sed -i.bak "s|releases/download/v[0-9.]*|releases/download/v$VERSION|" "$MANIFEST_PATH"

# Remove backup file
rm -f "${MANIFEST_PATH}.bak"

echo "✅ Manifest updated"
echo ""

# Check GitHub release
RELEASE_URL="https://github.com/nile-agi/delta/releases/download/v$VERSION/delta-cli-windows-x64.zip"
echo "🔍 Checking GitHub release..."

if curl -s --head --fail "$RELEASE_URL" > /dev/null 2>&1; then
    echo "✅ GitHub release found: $RELEASE_URL"
else
    echo "⚠️  GitHub release not found: $RELEASE_URL"
    echo "   The release must exist before submitting to winget."
    echo ""
    echo "💡 To create the release:"
    echo "   1. Go to: https://github.com/nile-agi/delta/releases/new"
    echo "   2. Tag: v$VERSION"
    echo "   3. Title: Delta CLI v$VERSION"
    echo "   4. Upload: $RELEASE_ZIP"
    echo "   5. Publish the release"
    echo ""
fi

echo ""
echo "✅ Preparation complete!"
echo ""
echo "📋 Next steps (on Windows):"
echo "   1. Run: .\packaging\winget\submit-to-winget.ps1 -Version $VERSION"
echo "   2. Or follow manual steps in: packaging/winget/SUBMIT.md"
echo ""

