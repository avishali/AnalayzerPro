#!/bin/bash

# Code Signing and Notarization Script for AnalyzerPro
# Signs and notarizes plugins for distribution on macOS

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
cd "$PROJECT_ROOT"

# Configuration
DEVELOPER_ID_APP="Developer ID Application: AVISHAY LIDANI (C5UC779LGC)"
DEVELOPER_ID_INSTALLER="Developer ID Installer: AVISHAY LIDANI (C5UC779LGC)"
APPLE_ID="avishay.lidani@gmail.com"
TEAM_ID="C5UC779LGC"
APP_SPECIFIC_PASSWORD="@keychain:AC_PASSWORD"  # Store in keychain!

# Plugin information
PLUGIN_NAME="AnalyzerPro"
PLUGIN_VERSION="1.1.1"
INSTALLER_DIR="$PROJECT_ROOT/installer"
BUILD_DIR="build-release"
ARTIFACTS_DIR="$BUILD_DIR/${PLUGIN_NAME}_artefacts/Release"

echo "=========================================="
echo "  AnalyzerPro Code Signing & Notarization"
echo "=========================================="
echo ""

# Check if artifacts exist
if [ ! -d "$ARTIFACTS_DIR" ]; then
    echo "❌ Error: Release artifacts not found!"
    echo "Please run ./scripts/build_release.sh first"
    exit 1
fi

STANDALONE_ENTITLEMENTS="$PROJECT_ROOT/resources/AnalyzerPro-standalone.entitlements"

echo "Step 1: Signing Plugins"
echo "========================"
echo ""

# Sign a bundle. Pass a second argument (entitlements path) for Hardened Runtime targets.
sign_bundle() {
    local bundle_path="$1"
    local entitlements="$2"
    local bundle_name=$(basename "$bundle_path")

    echo "🔏 Signing: $bundle_name"

    local entitlements_flag=""
    if [ -n "$entitlements" ]; then
        entitlements_flag="--entitlements $entitlements"
    fi

    codesign --deep --force --verify --verbose \
        --sign "$DEVELOPER_ID_APP" \
        --options runtime \
        --timestamp \
        $entitlements_flag \
        "$bundle_path"

    if [ $? -eq 0 ]; then
        echo "✅ Signed: $bundle_name"
        codesign --verify --deep --strict --verbose=2 "$bundle_path"
        spctl --assess --verbose=4 --type install "$bundle_path" 2>&1 | head -n 3
    else
        echo "❌ Failed to sign: $bundle_name"
        exit 1
    fi
    echo ""
}

# Sign AU
if [ -d "$ARTIFACTS_DIR/AU/${PLUGIN_NAME}.component" ]; then
    sign_bundle "$ARTIFACTS_DIR/AU/${PLUGIN_NAME}.component"
fi

# Sign VST3
if [ -d "$ARTIFACTS_DIR/VST3/${PLUGIN_NAME}.vst3" ]; then
    sign_bundle "$ARTIFACTS_DIR/VST3/${PLUGIN_NAME}.vst3"
fi

# Sign Standalone — must include entitlements so Hardened Runtime allows mic access
if [ -d "$ARTIFACTS_DIR/Standalone/${PLUGIN_NAME}.app" ]; then
    sign_bundle "$ARTIFACTS_DIR/Standalone/${PLUGIN_NAME}.app" "$STANDALONE_ENTITLEMENTS"
fi

# Sign AAX (if present)
if [ -d "$ARTIFACTS_DIR/AAX/${PLUGIN_NAME}.aaxplugin" ]; then
    echo "⚠️  Note: AAX requires additional PACE signing for Pro Tools"
    sign_bundle "$ARTIFACTS_DIR/AAX/${PLUGIN_NAME}.aaxplugin"
fi

echo ""
echo "✅ All plugins signed successfully!"
echo ""

echo "Step 2: Creating Signed Installer"
echo "=================================="
echo ""

# Recreate installer with signed plugins
echo "🔄 Rebuilding installer with signed plugins..."
"$SCRIPT_DIR/create_installer.sh"

# Sign the installer package
UNSIGNED_PKG="$INSTALLER_DIR/${PLUGIN_NAME}-${PLUGIN_VERSION}-macOS.pkg"
SIGNED_PKG="$INSTALLER_DIR/${PLUGIN_NAME}-${PLUGIN_VERSION}-macOS-signed.pkg"

echo ""
echo "🔏 Signing installer package..."

productsign --sign "$DEVELOPER_ID_INSTALLER" \
    "$UNSIGNED_PKG" \
    "$SIGNED_PKG"

if [ $? -eq 0 ]; then
    echo "✅ Installer signed successfully!"
    
    # Verify
    pkgutil --check-signature "$SIGNED_PKG"
else
    echo "❌ Failed to sign installer"
    exit 1
fi

echo ""
echo "Step 3: Notarization"
echo "===================="
echo ""

echo "📤 Submitting for notarization..."
echo "   This may take several minutes..."
echo ""

# Submit for notarization
xcrun notarytool submit "$SIGNED_PKG" \
    --keychain-profile "$APP_SPECIFIC_PASSWORD" \
    --wait

if [ $? -eq 0 ]; then
    echo ""
    echo "✅ Notarization successful!"
    echo ""
    
    # Staple the notarization ticket
    echo "📎 Stapling notarization ticket..."
    xcrun stapler staple "$SIGNED_PKG"
    
    if [ $? -eq 0 ]; then
        echo "✅ Notarization ticket stapled!"
        
        # Verify stapling
        xcrun stapler validate "$SIGNED_PKG"
    else
        echo "❌ Failed to staple notarization ticket"
        exit 1
    fi
else
    echo "❌ Notarization failed"
    echo ""
    echo "To check notarization status:"
    echo "  xcrun notarytool log REQUEST_ID --keychain-profile AC_PASSWORD"
    exit 1
fi

echo ""
echo "Step 4: Creating Distribution Archives"
echo "======================================="
echo ""

# Create signed ZIP
SIGNED_ZIP="$INSTALLER_DIR/${PLUGIN_NAME}-${PLUGIN_VERSION}-macOS-signed.zip"
cd "$ARTIFACTS_DIR"
zip -r -q "$SIGNED_ZIP" .
cd "$PROJECT_ROOT"

echo "✅ Signed ZIP created: $(basename "$SIGNED_ZIP")"
echo ""

# Create signed DMG
SIGNED_DMG="$INSTALLER_DIR/${PLUGIN_NAME}-${PLUGIN_VERSION}-macOS-signed.dmg"
TEMP_DMG_DIR="/tmp/${PLUGIN_NAME}-dmg"

rm -rf "$TEMP_DMG_DIR"
mkdir -p "$TEMP_DMG_DIR"
cp -R "$ARTIFACTS_DIR/"* "$TEMP_DMG_DIR/"

hdiutil create \
    -volname "${PLUGIN_NAME} ${PLUGIN_VERSION}" \
    -srcfolder "$TEMP_DMG_DIR" \
    -ov \
    -format UDZO \
    "$SIGNED_DMG"

rm -rf "$TEMP_DMG_DIR"

echo "✅ Signed DMG created: $(basename "$SIGNED_DMG")"
echo ""

echo "=========================================="
echo "  Distribution Complete!"
echo "=========================================="
echo ""
echo "✅ Signed & Notarized Package:"
echo "   $SIGNED_PKG"
echo ""
echo "✅ Signed ZIP Archive:"
echo "   $SIGNED_ZIP"
echo ""
echo "✅ Signed DMG:"
echo "   $SIGNED_DMG"
echo ""
echo "These files are ready for distribution!"
echo ""
echo "Recommended distribution checklist:"
echo "  ☐ Test installation on clean macOS system"
echo "  ☐ Test in multiple DAWs (Logic, Ableton, Pro Tools, etc.)"
echo "  ☐ Verify Gatekeeper acceptance"
echo "  ☐ Test on both Intel and Apple Silicon Macs"
echo "  ☐ Upload to distribution platform"
echo ""
