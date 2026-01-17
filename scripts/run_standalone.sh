#!/bin/bash
# Run the standalone AnalyzerPro app
# This script launches the binary directly so you can see debug output in the terminal.

APP_PATH="build/AnalyzerPro_artefacts/Release/Standalone/AnalyzerPro.app/Contents/MacOS/AnalyzerPro"

if [ -f "$APP_PATH" ]; then
    echo "Launching AnalyzerPro..."
    "$APP_PATH"
else
    echo "Error: App not found at $APP_PATH"
    echo "Please build the project first."
    exit 1
fi
