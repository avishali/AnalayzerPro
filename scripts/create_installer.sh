#!/bin/bash

# Installer Creation Script for AnalyzerPro
# Creates distributable installer packages for macOS

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
cd "$PROJECT_ROOT"

# Configuration
PLUGIN_NAME="AnalyzerPro"
PLUGIN_VERSION="1.0.0"
COMPANY_NAME="MelecDSP"
BUILD_DIR="build-release"
ARTIFACTS_DIR="$BUILD_DIR/${PLUGIN_NAME}_artefacts/Release"
INSTALLER_DIR="$PROJECT_ROOT/installer"
TEMP_DIR="$INSTALLER_DIR/temp"
PAYLOAD_DIR="$TEMP_DIR/payload"

echo "=========================================="
echo "  AnalyzerPro Installer Creator"
echo "=========================================="
echo "  Version: $PLUGIN_VERSION"
echo "=========================================="
echo ""

# Check if release build exists
if [ ! -d "$ARTIFACTS_DIR" ]; then
    echo "❌ Error: Release artifacts not found!"
    echo "Please run ./scripts/build_release.sh first"
    exit 1
fi

# Clean and create installer directories
echo "🧹 Preparing installer directories..."
rm -rf "$INSTALLER_DIR"
mkdir -p "$PAYLOAD_DIR"

# Create standard plugin directories in payload
mkdir -p "$PAYLOAD_DIR/Library/Audio/Plug-Ins/Components"
mkdir -p "$PAYLOAD_DIR/Library/Audio/Plug-Ins/VST3"
mkdir -p "$PAYLOAD_DIR/Applications"

# Copy plugins to payload
echo ""
echo "📦 Packaging plugins..."
echo ""

# Copy AU
if [ -d "$ARTIFACTS_DIR/AU/${PLUGIN_NAME}.component" ]; then
    echo "  ✓ Copying AU..."
    cp -R "$ARTIFACTS_DIR/AU/${PLUGIN_NAME}.component" "$PAYLOAD_DIR/Library/Audio/Plug-Ins/Components/"
fi

# Copy VST3
if [ -d "$ARTIFACTS_DIR/VST3/${PLUGIN_NAME}.vst3" ]; then
    echo "  ✓ Copying VST3..."
    cp -R "$ARTIFACTS_DIR/VST3/${PLUGIN_NAME}.vst3" "$PAYLOAD_DIR/Library/Audio/Plug-Ins/VST3/"
fi

# Copy Standalone
if [ -d "$ARTIFACTS_DIR/Standalone/${PLUGIN_NAME}.app" ]; then
    echo "  ✓ Copying Standalone..."
    cp -R "$ARTIFACTS_DIR/Standalone/${PLUGIN_NAME}.app" "$PAYLOAD_DIR/Applications/"
fi

# AAX requires special handling (Pro Tools signing)
if [ -d "$ARTIFACTS_DIR/AAX/${PLUGIN_NAME}.aaxplugin" ]; then
    echo "  ⚠️  AAX found but requires PACE signing for distribution"
    echo "     (AAX will be packaged separately after signing)"
fi

echo ""
echo "✓ Plugins packaged"
echo ""

# Create README for installer
cat > "$PAYLOAD_DIR/README.txt" << EOF
${PLUGIN_NAME} v${PLUGIN_VERSION}
${COMPANY_NAME}

INSTALLATION
============

This installer will copy the following components:

1. AU (Audio Unit): ~/Library/Audio/Plug-Ins/Components/
2. VST3: ~/Library/Audio/Plug-Ins/VST3/
3. Standalone App: /Applications/

SYSTEM REQUIREMENTS
===================

- macOS 10.13 or later
- Intel or Apple Silicon Mac
- Compatible DAW (for AU/VST3 plugins)

MICROPHONE PERMISSION
=====================

The Standalone application requires microphone access to capture audio 
from input devices, including virtual loopback devices like BlackHole.
You will be prompted to grant permission on first launch.

SUPPORT
=======

For support, visit: https://www.melechdsp.com

Copyright © $(date +%Y) ${COMPANY_NAME}. All rights reserved.
EOF

# Create distribution package using pkgbuild
echo "🔨 Creating macOS installer package..."
echo ""

PKG_OUTPUT="$INSTALLER_DIR/${PLUGIN_NAME}-${PLUGIN_VERSION}-macOS.pkg"

# Create component plist for better compatibility with macOS 15+
COMPONENT_PLIST="$TEMP_DIR/component.plist"
cat > "$COMPONENT_PLIST" << EOF
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<array>
    <dict>
        <key>BundleIsRelocatable</key>
        <false/>
        <key>BundleIsVersionChecked</key>
        <false/>
        <key>BundleHasStrictIdentifier</key>
        <false/>
        <key>BundleOverwriteAction</key>
        <string>upgrade</string>
        <key>RootRelativeBundlePath</key>
        <string>Library/Audio/Plug-Ins/Components/${PLUGIN_NAME}.component</string>
    </dict>
</array>
</plist>
EOF

pkgbuild \
    --root "$PAYLOAD_DIR" \
    --identifier "com.melechdsp.${PLUGIN_NAME}" \
    --version "$PLUGIN_VERSION" \
    --install-location "/" \
    --component-plist "$COMPONENT_PLIST" \
    --min-os-version "10.13" \
    "$PKG_OUTPUT"

if [ $? -ne 0 ]; then
    echo "❌ Package creation failed!"
    exit 1
fi

echo ""
echo "✅ Installer package created!"
echo ""

# Create ZIP archive as alternative distribution
echo "📦 Creating ZIP archive..."
ZIP_OUTPUT="$INSTALLER_DIR/${PLUGIN_NAME}-${PLUGIN_VERSION}-macOS.zip"

cd "$PAYLOAD_DIR"
zip -r -q "$ZIP_OUTPUT" .
cd "$PROJECT_ROOT"

echo "✅ ZIP archive created!"
echo ""

# Create DMG (optional - requires hdiutil)
echo "💿 Creating DMG disk image..."
DMG_OUTPUT="$INSTALLER_DIR/${PLUGIN_NAME}-${PLUGIN_VERSION}-macOS.dmg"
DMG_TEMP="$TEMP_DIR/dmg"

mkdir -p "$DMG_TEMP"
cp -R "$PAYLOAD_DIR/"* "$DMG_TEMP/"

# Create a nice DMG layout
cat > "$DMG_TEMP/INSTALL.txt" << EOF
${PLUGIN_NAME} v${PLUGIN_VERSION}

INSTALLATION INSTRUCTIONS
=========================

1. Copy ${PLUGIN_NAME}.component to:
   ~/Library/Audio/Plug-Ins/Components/

2. Copy ${PLUGIN_NAME}.vst3 to:
   ~/Library/Audio/Plug-Ins/VST3/

3. Copy ${PLUGIN_NAME}.app to:
   /Applications/

4. Restart your DAW

For easier installation, you can also use the included .pkg installer.

---
${COMPANY_NAME} © $(date +%Y)
EOF

hdiutil create \
    -volname "${PLUGIN_NAME} ${PLUGIN_VERSION}" \
    -srcfolder "$DMG_TEMP" \
    -ov \
    -format UDZO \
    "$DMG_OUTPUT" \
    2>/dev/null

if [ $? -eq 0 ]; then
    echo "✅ DMG created!"
else
    echo "⚠️  DMG creation skipped (requires hdiutil)"
fi

echo ""

# Clean up temp files
echo "🧹 Cleaning up..."
rm -rf "$TEMP_DIR"

# Summary
echo ""
echo "=========================================="
echo "  Distribution Packages Created"
echo "=========================================="
echo ""
echo "📦 Installer Package:"
echo "   $PKG_OUTPUT"
echo ""
echo "📦 ZIP Archive:"
echo "   $ZIP_OUTPUT"
echo ""
if [ -f "$DMG_OUTPUT" ]; then
    echo "💿 DMG Disk Image:"
    echo "   $DMG_OUTPUT"
    echo ""
fi
echo "=========================================="
echo ""
echo "⚠️  IMPORTANT: Code Signing & Notarization"
echo "=========================================="
echo ""
echo "Before distributing to customers:"
echo ""
echo "1. Sign all binaries with your Developer ID:"
echo "   codesign --deep --force --verify --verbose \\"
echo "     --sign \"Developer ID Application: Your Name\" \\"
echo "     --options runtime \\"
echo "     path/to/plugin"
echo ""
echo "2. Sign the installer package:"
echo "   productsign --sign \"Developer ID Installer: Your Name\" \\"
echo "     unsigned.pkg signed.pkg"
echo ""
echo "3. Notarize with Apple:"
echo "   xcrun notarytool submit signed.pkg \\"
echo "     --apple-id your@email.com \\"
echo "     --team-id TEAMID \\"
echo "     --password app-specific-password \\"
echo "     --wait"
echo ""
echo "4. Staple the notarization ticket:"
echo "   xcrun stapler staple signed.pkg"
echo ""
echo "For detailed signing instructions, see:"
echo "https://developer.apple.com/documentation/security/notarizing_macos_software_before_distribution"
echo ""
echo "=========================================="
echo ""
echo "✅ Installer creation complete!"
echo ""
