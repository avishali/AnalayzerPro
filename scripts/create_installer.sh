#!/bin/bash

# Installer Creation Script for AnalyzerPro
# Creates distributable installer packages for macOS
#
# INSTALL_DOMAIN=user|system — if unset and the build includes AAX, defaults to system so AAX
# installs to /Library/Application Support/Avid/Audio/Plug-Ins (Avid machine-wide path).
# Set INSTALL_DOMAIN=user to keep AU/VST3/AAX under the installing user's home Library instead.
# ANALYZERPRO_AAX_USE_SYSTEM_WIDE_LIBRARY=0 — when INSTALL_DOMAIN is unset, default user-only even if AAX exists.

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
cd "$PROJECT_ROOT"

# Configuration
PLUGIN_NAME="AnalyzerPro"
# Derived from CMakeLists.txt (single source of truth) — see scripts/plugin_version.sh
source "$SCRIPT_DIR/plugin_version.sh"
COMPANY_NAME="MelecDSP"
# Must match BUILD_DIR in scripts/build_release.sh and scripts/sign_and_notarize.sh
BUILD_DIR="${ANALYZERPRO_RELEASE_BUILD_DIR:-build-release}"
ARTIFACTS_DIR="$BUILD_DIR/${PLUGIN_NAME}_artefacts/Release"
INSTALLER_DIR="$PROJECT_ROOT/installer"
TEMP_DIR="$INSTALLER_DIR/temp"
PAYLOAD_DIR="$TEMP_DIR/payload"
# INSTALL_DOMAIN resolved after we detect AAX (see below).

# Check if release build exists
if [ ! -d "$ARTIFACTS_DIR" ]; then
    echo "❌ Error: Release artifacts not found!"
    echo "Please run ./scripts/build_release.sh first"
    exit 1
fi

HAVE_AAX=0
if [ -d "$ARTIFACTS_DIR/AAX/${PLUGIN_NAME}.aaxplugin" ]; then
    HAVE_AAX=1
fi

# Avid’s usual machine-wide AAX folder is /Library/Application Support/Avid/Audio/Plug-Ins.
# A single Installer product cannot mix “user home” payloads and “/Library” payloads; when AAX is
# bundled we default to a system-domain installer so AAX lands on the system volume.
# Override: INSTALL_DOMAIN=user (AAX then goes under the installing user’s ~/Library/...), or
# ANALYZERPRO_AAX_USE_SYSTEM_WIDE_LIBRARY=0 with INSTALL_DOMAIN unset to default user-only.
ANALYZERPRO_AAX_USE_SYSTEM_WIDE_LIBRARY="${ANALYZERPRO_AAX_USE_SYSTEM_WIDE_LIBRARY:-1}"

if [ "${INSTALL_DOMAIN+x}" = x ]; then
    INSTALL_DOMAIN="${INSTALL_DOMAIN:-user}"
else
    if [ "$HAVE_AAX" -eq 1 ] && [ "$ANALYZERPRO_AAX_USE_SYSTEM_WIDE_LIBRARY" != "0" ]; then
        INSTALL_DOMAIN=system
    else
        INSTALL_DOMAIN=user
    fi
fi

if [ "${INSTALL_DOMAIN+x}" = x ] && [ "$INSTALL_DOMAIN" = "user" ] && [ "$HAVE_AAX" -eq 1 ]; then
    echo "⚠️  INSTALL_DOMAIN=user: AAX will install under ~/Library/Application Support/Avid/... (per user)."
    echo "    For /Library/Application Support/Avid/Audio/Plug-Ins use INSTALL_DOMAIN=system or unset INSTALL_DOMAIN."
    echo ""
fi

echo "=========================================="
echo "  AnalyzerPro Installer Creator"
echo "=========================================="
echo "  Version: $PLUGIN_VERSION"
echo "  Install domain: $INSTALL_DOMAIN (user → ~/Library…; system → /Library… / /Applications…)"
if [ "$HAVE_AAX" -eq 1 ] && [ "$INSTALL_DOMAIN" = "system" ]; then
    echo "  AAX → /Library/Application Support/Avid/Audio/Plug-Ins/ (system-wide; admin install)"
fi
echo "=========================================="
echo ""

# Clean and create installer directories
echo "🧹 Preparing installer directories..."
rm -rf "$INSTALLER_DIR"
mkdir -p "$PAYLOAD_DIR"

# Paths inside the pkg root (same layout for user vs system). Installer domain decides the root:
#   user:   enable_currentUserHome="true"  + install-location "/" → ~/Library/..., ~/Applications/...
#   system: enable_localSystem="true"       + install-location "/" → /Library/..., /Applications/...
# Do NOT use --install-location "$HOME" or the literal "$HOME" string — files can end up in the wrong place.
DEST_COMPONENTS="Library/Audio/Plug-Ins/Components"
DEST_VST3="Library/Audio/Plug-Ins/VST3"
DEST_APPS="Applications"
DEST_AAX="Library/Application Support/Avid/Audio/Plug-Ins"
PKG_INSTALL_ROOT="/"

if [ "$INSTALL_DOMAIN" = "system" ]; then
    DOMAIN_LINE='<domains enable_anywhere="false" enable_currentUserHome="false" enable_localSystem="true"/>'
    DOC_PATH_AU="/Library/Audio/Plug-Ins/Components/"
    DOC_PATH_VST3="/Library/Audio/Plug-Ins/VST3/"
    DOC_PATH_APP="/Applications/"
    DOC_PATH_AAX="/Library/Application Support/Avid/Audio/Plug-Ins/"
else
    DOMAIN_LINE='<domains enable_anywhere="false" enable_currentUserHome="true" enable_localSystem="false"/>'
    DOC_PATH_AU="~/Library/Audio/Plug-Ins/Components/"
    DOC_PATH_VST3="~/Library/Audio/Plug-Ins/VST3/"
    DOC_PATH_APP="~/Applications/"
    DOC_PATH_AAX="~/Library/Application Support/Avid/Audio/Plug-Ins/"
fi

mkdir -p "$PAYLOAD_DIR/$DEST_COMPONENTS"
mkdir -p "$PAYLOAD_DIR/$DEST_VST3"
mkdir -p "$PAYLOAD_DIR/$DEST_APPS"
mkdir -p "$PAYLOAD_DIR/$DEST_AAX"

# Copy plugins to payload
echo ""
echo "📦 Packaging plugins..."
echo ""

# Copy AU
if [ -d "$ARTIFACTS_DIR/AU/${PLUGIN_NAME}.component" ]; then
    echo "  ✓ Copying AU..."
    cp -R "$ARTIFACTS_DIR/AU/${PLUGIN_NAME}.component" "$PAYLOAD_DIR/$DEST_COMPONENTS/"
fi

# Copy VST3
if [ -d "$ARTIFACTS_DIR/VST3/${PLUGIN_NAME}.vst3" ]; then
    echo "  ✓ Copying VST3..."
    cp -R "$ARTIFACTS_DIR/VST3/${PLUGIN_NAME}.vst3" "$PAYLOAD_DIR/$DEST_VST3/"
fi

# Copy Standalone
if [ -d "$ARTIFACTS_DIR/Standalone/${PLUGIN_NAME}.app" ]; then
    echo "  ✓ Copying Standalone..."
    cp -R "$ARTIFACTS_DIR/Standalone/${PLUGIN_NAME}.app" "$PAYLOAD_DIR/$DEST_APPS/"
fi

# Copy AAX (PACE-signed build expected before packaging for Pro Tools; see docs/aax_pace.md)
if [ -d "$ARTIFACTS_DIR/AAX/${PLUGIN_NAME}.aaxplugin" ]; then
    echo "  ✓ Copying AAX (Pro Tools)..."
    cp -R "$ARTIFACTS_DIR/AAX/${PLUGIN_NAME}.aaxplugin" "$PAYLOAD_DIR/$DEST_AAX/"
fi

echo ""
echo "✓ Plugins packaged"
echo ""

README_SYS_NOTE=""
if [ "$INSTALL_DOMAIN" = "system" ]; then
    README_SYS_NOTE="Administrator password required. Plugins install to the system Library and /Applications (AAX: /Library/Application Support/Avid/Audio/Plug-Ins).

"
fi

# Create README for installer
cat > "$PAYLOAD_DIR/README.txt" << EOF
${PLUGIN_NAME} v${PLUGIN_VERSION}
${COMPANY_NAME}

INSTALLATION
============

${README_SYS_NOTE}Use the Installer "Customize" step to choose formats. Default install locations:

1. AU (Audio Unit): ${DOC_PATH_AU}
2. VST3: ${DOC_PATH_VST3}
3. Standalone App: ${DOC_PATH_APP}
4. AAX (Pro Tools, if included): ${DOC_PATH_AAX}

Use a PACE-signed AAX build before creating this installer (see scripts/wraptool_sign_aax.sh and docs/aax_pace.md).

SYSTEM REQUIREMENTS
===================

- macOS 10.15 or later
- Intel or Apple Silicon Mac
- Compatible DAW (for AU/VST3/AAX plugins)

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

# Create distribution package: one flat pkg per format + productbuild (Customize to pick formats).
echo "🔨 Creating macOS installer package (use Customize to select AU / VST3 / Standalone / AAX)…"
echo ""

PKG_OUTPUT="$INSTALLER_DIR/${PLUGIN_NAME}-${PLUGIN_VERSION}-macOS.pkg"
SUBROOTS="$TEMP_DIR/subroots"
rm -rf "$SUBROOTS"
mkdir -p "$SUBROOTS"

write_one_bundle_plist() {
  local plist_out="$1"
  local rel_path="$2"
  cat > "$plist_out" << PLISTEOF
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
        <string>${rel_path}</string>
    </dict>
</array>
</plist>
PLISTEOF
}

build_subpkg() {
  local format_id="$1"
  local pkg_file="$2"
  local dest_subdir="$3"
  local bundle_name="$4"
  local source_path="$5"
  local rel_in_pkg="${dest_subdir}/${bundle_name}"

  local root="${SUBROOTS}/${format_id}"
  mkdir -p "${root}/${dest_subdir}"
  cp -R "${source_path}" "${root}/${dest_subdir}/"

  local plist="${TEMP_DIR}/component-${format_id}.plist"
  write_one_bundle_plist "${plist}" "${rel_in_pkg}"

  pkgbuild \
    --root "${root}" \
    --identifier "com.melechdsp.analyzerpro.${format_id}" \
    --version "${PLUGIN_VERSION}" \
    --install-location "${PKG_INSTALL_ROOT}" \
    --component-plist "${plist}" \
    --min-os-version "10.15" \
    "${TEMP_DIR}/${pkg_file}"
}

SUBPKG_COUNT=0
if [ -d "$ARTIFACTS_DIR/AU/${PLUGIN_NAME}.component" ]; then
  echo "  ✓ Flat package: AU"
  build_subpkg "au" "${PLUGIN_NAME}-AU.pkg" "${DEST_COMPONENTS}" "${PLUGIN_NAME}.component" "$ARTIFACTS_DIR/AU/${PLUGIN_NAME}.component"
  SUBPKG_COUNT=$((SUBPKG_COUNT + 1))
fi
if [ -d "$ARTIFACTS_DIR/VST3/${PLUGIN_NAME}.vst3" ]; then
  echo "  ✓ Flat package: VST3"
  build_subpkg "vst3" "${PLUGIN_NAME}-VST3.pkg" "${DEST_VST3}" "${PLUGIN_NAME}.vst3" "$ARTIFACTS_DIR/VST3/${PLUGIN_NAME}.vst3"
  SUBPKG_COUNT=$((SUBPKG_COUNT + 1))
fi
if [ -d "$ARTIFACTS_DIR/Standalone/${PLUGIN_NAME}.app" ]; then
  echo "  ✓ Flat package: Standalone"
  build_subpkg "standalone" "${PLUGIN_NAME}-Standalone.pkg" "${DEST_APPS}" "${PLUGIN_NAME}.app" "$ARTIFACTS_DIR/Standalone/${PLUGIN_NAME}.app"
  SUBPKG_COUNT=$((SUBPKG_COUNT + 1))
fi
if [ -d "$ARTIFACTS_DIR/AAX/${PLUGIN_NAME}.aaxplugin" ]; then
  echo "  ✓ Flat package: AAX"
  build_subpkg "aax" "${PLUGIN_NAME}-AAX.pkg" "${DEST_AAX}" "${PLUGIN_NAME}.aaxplugin" "$ARTIFACTS_DIR/AAX/${PLUGIN_NAME}.aaxplugin"
  SUBPKG_COUNT=$((SUBPKG_COUNT + 1))
fi

if [ "$SUBPKG_COUNT" -eq 0 ]; then
  echo "❌ No plugin formats found under $ARTIFACTS_DIR; nothing to package."
  exit 1
fi

DIST_XML="${TEMP_DIR}/Distribution.xml"
{
  echo '<?xml version="1.0" encoding="utf-8"?>'
  echo '<installer-gui-script minSpecVersion="2">'
  echo "  <title>${PLUGIN_NAME} ${PLUGIN_VERSION}</title>"
  echo '  <options customize="always" require-scripts="false"/>'
  echo "  ${DOMAIN_LINE}"
  echo '  <choices-outline>'
  [ -f "${TEMP_DIR}/${PLUGIN_NAME}-AU.pkg" ] && echo '    <line choice="fmt_au"/>'
  [ -f "${TEMP_DIR}/${PLUGIN_NAME}-VST3.pkg" ] && echo '    <line choice="fmt_vst3"/>'
  [ -f "${TEMP_DIR}/${PLUGIN_NAME}-Standalone.pkg" ] && echo '    <line choice="fmt_standalone"/>'
  [ -f "${TEMP_DIR}/${PLUGIN_NAME}-AAX.pkg" ] && echo '    <line choice="fmt_aax"/>'
  echo '  </choices-outline>'
  if [ -f "${TEMP_DIR}/${PLUGIN_NAME}-AU.pkg" ]; then
    printf '  <choice id="fmt_au" visible="true" title="Audio Unit (AU)" description="Logic, GarageBand, and other AU hosts. Installs to: %s" start_selected="true">\n' "${DOC_PATH_AU}"
    echo '    <pkg-ref id="com.melechdsp.analyzerpro.au"/>'
    echo '  </choice>'
  fi
  if [ -f "${TEMP_DIR}/${PLUGIN_NAME}-VST3.pkg" ]; then
    printf '  <choice id="fmt_vst3" visible="true" title="VST3" description="Ableton, Cubase, Reaper, and other VST3 hosts. Installs to: %s" start_selected="true">\n' "${DOC_PATH_VST3}"
    echo '    <pkg-ref id="com.melechdsp.analyzerpro.vst3"/>'
    echo '  </choice>'
  fi
  if [ -f "${TEMP_DIR}/${PLUGIN_NAME}-Standalone.pkg" ]; then
    printf '  <choice id="fmt_standalone" visible="true" title="Standalone application" description="AnalyzerPro app with microphone access for input capture. Installs to: %s" start_selected="true">\n' "${DOC_PATH_APP}"
    echo '    <pkg-ref id="com.melechdsp.analyzerpro.standalone"/>'
    echo '  </choice>'
  fi
  if [ -f "${TEMP_DIR}/${PLUGIN_NAME}-AAX.pkg" ]; then
    printf '  <choice id="fmt_aax" visible="true" title="AAX (Pro Tools)" description="Pro Tools and AAX hosts. PACE-signed build required. Installs to: %s" start_selected="true">\n' "${DOC_PATH_AAX}"
    echo '    <pkg-ref id="com.melechdsp.analyzerpro.aax"/>'
    echo '  </choice>'
  fi
  [ -f "${TEMP_DIR}/${PLUGIN_NAME}-AU.pkg" ] && echo "  <pkg-ref id=\"com.melechdsp.analyzerpro.au\" version=\"${PLUGIN_VERSION}\" onConclusion=\"none\">${PLUGIN_NAME}-AU.pkg</pkg-ref>"
  [ -f "${TEMP_DIR}/${PLUGIN_NAME}-VST3.pkg" ] && echo "  <pkg-ref id=\"com.melechdsp.analyzerpro.vst3\" version=\"${PLUGIN_VERSION}\" onConclusion=\"none\">${PLUGIN_NAME}-VST3.pkg</pkg-ref>"
  [ -f "${TEMP_DIR}/${PLUGIN_NAME}-Standalone.pkg" ] && echo "  <pkg-ref id=\"com.melechdsp.analyzerpro.standalone\" version=\"${PLUGIN_VERSION}\" onConclusion=\"none\">${PLUGIN_NAME}-Standalone.pkg</pkg-ref>"
  [ -f "${TEMP_DIR}/${PLUGIN_NAME}-AAX.pkg" ] && echo "  <pkg-ref id=\"com.melechdsp.analyzerpro.aax\" version=\"${PLUGIN_VERSION}\" onConclusion=\"none\">${PLUGIN_NAME}-AAX.pkg</pkg-ref>"
  echo '</installer-gui-script>'
} > "${DIST_XML}"

if ! productbuild \
    --distribution "${DIST_XML}" \
    --package-path "${TEMP_DIR}" \
    "${PKG_OUTPUT}"; then
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
   ${DOC_PATH_AU}

2. Copy ${PLUGIN_NAME}.vst3 to:
   ${DOC_PATH_VST3}

3. Copy ${PLUGIN_NAME}.app to:
   ${DOC_PATH_APP}

4. Copy ${PLUGIN_NAME}.aaxplugin (if present) to:
   ${DOC_PATH_AAX}

5. Restart your DAW

For easier installation, use the .pkg installer: click Customize to choose AU, VST3, Standalone, and/or AAX.

AAX must be PACE-signed for Pro Tools distribution; do not re-sign the inner binary with Apple after PACE (see docs/aax_pace.md).

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
echo "0. AAX (if packaged): PACE-sign with wraptool before installer/notarization;"
echo "   use SIGN_AND_NOTARIZE_SKIP_AAX=1 when running scripts/sign_and_notarize.sh"
echo "   so Apple does not re-sign the AAX inner binary (see docs/aax_pace.md)."
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
