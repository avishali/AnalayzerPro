#!/bin/bash

# Simple Installer Creator for AnalyzerPro
# Creates a self-contained installation script that works on all macOS versions
# This bypasses PKG compatibility issues on macOS 15+

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
cd "$PROJECT_ROOT"

# Configuration
PLUGIN_NAME="AnalyzerPro"
# Derived from CMakeLists.txt (single source of truth) — see scripts/plugin_version.sh
source "$SCRIPT_DIR/plugin_version.sh"
COMPANY_NAME="MelecDSP"
BUILD_DIR="build-release"
ARTIFACTS_DIR="$BUILD_DIR/${PLUGIN_NAME}_artefacts/Release"
INSTALLER_DIR="$PROJECT_ROOT/installer"
PACKAGE_DIR="$INSTALLER_DIR/${PLUGIN_NAME}-${PLUGIN_VERSION}-Simple"

echo "=========================================="
echo "  AnalyzerPro Simple Installer Creator"
echo "=========================================="
echo "  Compatible with all macOS versions"
echo "  Including macOS 15 (Sequoia)"
echo "=========================================="
echo ""

# Check if release build exists
if [ ! -d "$ARTIFACTS_DIR" ]; then
    echo "❌ Error: Release artifacts not found!"
    echo "Please run ./scripts/build_release.sh first"
    exit 1
fi

# Clean and create package directory
echo "🧹 Preparing package directory..."
rm -rf "$PACKAGE_DIR"
mkdir -p "$PACKAGE_DIR"

# Copy plugins to package
echo ""
echo "📦 Copying plugins..."
echo ""

if [ -d "$ARTIFACTS_DIR/AU/${PLUGIN_NAME}.component" ]; then
    echo "  ✓ Copying AU..."
    mkdir -p "$PACKAGE_DIR/Plugins/AU"
    cp -R "$ARTIFACTS_DIR/AU/${PLUGIN_NAME}.component" "$PACKAGE_DIR/Plugins/AU/"
fi

if [ -d "$ARTIFACTS_DIR/VST3/${PLUGIN_NAME}.vst3" ]; then
    echo "  ✓ Copying VST3..."
    mkdir -p "$PACKAGE_DIR/Plugins/VST3"
    cp -R "$ARTIFACTS_DIR/VST3/${PLUGIN_NAME}.vst3" "$PACKAGE_DIR/Plugins/VST3/"
fi

if [ -d "$ARTIFACTS_DIR/Standalone/${PLUGIN_NAME}.app" ]; then
    echo "  ✓ Copying Standalone..."
    mkdir -p "$PACKAGE_DIR/Standalone"
    cp -R "$ARTIFACTS_DIR/Standalone/${PLUGIN_NAME}.app" "$PACKAGE_DIR/Standalone/"
fi

echo ""
echo "✓ Plugins copied"
echo ""

# Create installation script
echo "📝 Creating installation script..."

cat > "$PACKAGE_DIR/INSTALL.command" << 'EOFINSTALL'
#!/bin/bash

# AnalyzerPro Installation Script
# Compatible with all macOS versions including macOS 15+

set -e

PLUGIN_NAME="AnalyzerPro"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

echo ""
echo "=========================================="
echo "  AnalyzerPro Installer"
echo "=========================================="
echo ""
echo "This will install:"
echo "  • AU Plugin → ~/Library/Audio/Plug-Ins/Components/"
echo "  • VST3 Plugin → ~/Library/Audio/Plug-Ins/VST3/"
echo "  • Standalone App → /Applications/"
echo ""
echo "Press RETURN to continue, or Ctrl+C to cancel"
read

echo ""
echo "Installing plugins..."
echo ""

# Create directories if they don't exist
mkdir -p ~/Library/Audio/Plug-Ins/Components
mkdir -p ~/Library/Audio/Plug-Ins/VST3

# Install AU
if [ -d "$SCRIPT_DIR/Plugins/AU/${PLUGIN_NAME}.component" ]; then
    echo "📦 Installing AU Plugin..."
    cp -R "$SCRIPT_DIR/Plugins/AU/${PLUGIN_NAME}.component" ~/Library/Audio/Plug-Ins/Components/
    echo "   ✓ Installed to: ~/Library/Audio/Plug-Ins/Components/"
fi

# Install VST3
if [ -d "$SCRIPT_DIR/Plugins/VST3/${PLUGIN_NAME}.vst3" ]; then
    echo "📦 Installing VST3 Plugin..."
    cp -R "$SCRIPT_DIR/Plugins/VST3/${PLUGIN_NAME}.vst3" ~/Library/Audio/Plug-Ins/VST3/
    echo "   ✓ Installed to: ~/Library/Audio/Plug-Ins/VST3/"
fi

# Install Standalone
if [ -d "$SCRIPT_DIR/Standalone/${PLUGIN_NAME}.app" ]; then
    echo "📦 Installing Standalone App..."
    cp -R "$SCRIPT_DIR/Standalone/${PLUGIN_NAME}.app" /Applications/
    echo "   ✓ Installed to: /Applications/"
fi

echo ""
echo "🔓 Removing quarantine attributes (allows macOS to run the plugins)..."
xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/Components/${PLUGIN_NAME}.component 2>/dev/null || true
xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/VST3/${PLUGIN_NAME}.vst3 2>/dev/null || true
xattr -dr com.apple.quarantine /Applications/${PLUGIN_NAME}.app 2>/dev/null || true

echo ""
echo "=========================================="
echo "  Installation Complete! ✅"
echo "=========================================="
echo ""
echo "Next steps:"
echo "  1. Restart your DAW (if it was open)"
echo "  2. Rescan plugins in DAW preferences"
echo "  3. Load ${PLUGIN_NAME} from your plugin list"
echo ""
echo "Standalone app is in: /Applications/${PLUGIN_NAME}.app"
echo ""
echo "Press RETURN to exit"
read
EOFINSTALL

chmod +x "$PACKAGE_DIR/INSTALL.command"

# Create README
cat > "$PACKAGE_DIR/README.txt" << EOF
${PLUGIN_NAME} v${PLUGIN_VERSION}
${COMPANY_NAME}

INSTALLATION INSTRUCTIONS
=========================

EASY METHOD (Recommended):
1. Double-click "INSTALL.command"
2. Press RETURN when prompted
3. Enter your Mac password if asked
4. Done!

MANUAL METHOD:
1. Copy "Plugins/AU/${PLUGIN_NAME}.component" to:
   ~/Library/Audio/Plug-Ins/Components/

2. Copy "Plugins/VST3/${PLUGIN_NAME}.vst3" to:
   ~/Library/Audio/Plug-Ins/VST3/

3. Copy "Standalone/${PLUGIN_NAME}.app" to:
   /Applications/

4. Remove quarantine (Terminal):
   xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/Components/${PLUGIN_NAME}.component
   xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/VST3/${PLUGIN_NAME}.vst3
   xattr -dr com.apple.quarantine /Applications/${PLUGIN_NAME}.app

AFTER INSTALLATION:
- Restart your DAW
- Rescan plugins
- Load ${PLUGIN_NAME}

SYSTEM REQUIREMENTS:
- macOS 10.13 or later
- Intel or Apple Silicon Mac
- Compatible DAW

TROUBLESHOOTING:
- Plugin not showing? Restart DAW and rescan plugins
- Security warning? Right-click plugin → Open
- Standalone won't launch? Check microphone permission in System Settings

SUPPORT:
For help, contact: support@melechdsp.com

${COMPANY_NAME} © $(date +%Y)
EOF

# Create uninstaller
cat > "$PACKAGE_DIR/UNINSTALL.command" << 'EOFUNINSTALL'
#!/bin/bash

# AnalyzerPro Uninstaller

PLUGIN_NAME="AnalyzerPro"

echo ""
echo "=========================================="
echo "  AnalyzerPro Uninstaller"
echo "=========================================="
echo ""
echo "This will remove:"
echo "  • ~/Library/Audio/Plug-Ins/Components/${PLUGIN_NAME}.component"
echo "  • ~/Library/Audio/Plug-Ins/VST3/${PLUGIN_NAME}.vst3"
echo "  • /Applications/${PLUGIN_NAME}.app"
echo ""
echo "Press RETURN to continue, or Ctrl+C to cancel"
read

echo ""
echo "Uninstalling..."

rm -rf ~/Library/Audio/Plug-Ins/Components/${PLUGIN_NAME}.component
rm -rf ~/Library/Audio/Plug-Ins/VST3/${PLUGIN_NAME}.vst3
rm -rf /Applications/${PLUGIN_NAME}.app

echo ""
echo "✅ Uninstallation complete!"
echo ""
echo "Press RETURN to exit"
read
EOFUNINSTALL

chmod +x "$PACKAGE_DIR/UNINSTALL.command"

echo ""
echo "✓ Installation scripts created"
echo ""

# Create ZIP archive
echo "📦 Creating ZIP archive..."
ZIP_OUTPUT="$INSTALLER_DIR/${PLUGIN_NAME}-${PLUGIN_VERSION}-macOS-Simple.zip"

cd "$INSTALLER_DIR"
zip -r -q "$(basename "$ZIP_OUTPUT")" "$(basename "$PACKAGE_DIR")"
cd "$PROJECT_ROOT"

echo "✅ ZIP archive created!"
echo ""

# Summary
echo "=========================================="
echo "  Package Created!"
echo "=========================================="
echo ""
echo "📁 Package folder:"
echo "   $PACKAGE_DIR"
echo ""
echo "📦 ZIP archive:"
echo "   $ZIP_OUTPUT"
echo ""
echo "=========================================="
echo "  Distribution Instructions"
echo "=========================================="
echo ""
echo "Send to users:"
echo "  • ${PLUGIN_NAME}-${PLUGIN_VERSION}-macOS-Simple.zip"
echo ""
echo "Installation (on their computer):"
echo "  1. Unzip the archive"
echo "  2. Double-click 'INSTALL.command'"
echo "  3. Press RETURN and enter password when prompted"
echo "  4. Done!"
echo ""
echo "✅ Works on ALL macOS versions (10.13 - 15+)"
echo "✅ No PKG compatibility issues"
echo "✅ Automatic quarantine removal"
echo ""
