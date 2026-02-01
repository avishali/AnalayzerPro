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
