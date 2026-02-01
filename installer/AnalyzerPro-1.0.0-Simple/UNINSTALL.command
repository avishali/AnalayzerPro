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
