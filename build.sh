#!/bin/bash

# Build script for AnalyzerPro
# Configures and builds the plugin (AU, VST3) and standalone application using CMake

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

BUILD_DIR="build"
CONFIG="${1:-Release}"

# Check if JUCE_PATH is set
if [ -z "$JUCE_PATH" ]; then
    echo "Error: JUCE_PATH environment variable is not set."
    echo "Please set it to your JUCE installation path:"
    echo "  export JUCE_PATH=/path/to/JUCE"
    echo "Or add it to your ~/.zshrc file"
    exit 1
fi

# Verify JUCE path exists
if [ ! -d "$JUCE_PATH" ]; then
    echo "Error: JUCE_PATH directory does not exist: $JUCE_PATH"
    exit 1
fi

echo "Building AnalyzerPro..."
echo "  Build directory: $BUILD_DIR"
echo "  Configuration: $CONFIG"
echo "  JUCE path: $JUCE_PATH"
echo "  Formats: AU, VST3, Standalone"
echo ""

# Configure CMake
echo "Configuring CMake..."
cmake -S . -B "$BUILD_DIR" \
    -DCMAKE_BUILD_TYPE="$CONFIG" \
    -DJUCE_PATH="$JUCE_PATH"

# Build
echo ""
echo "Building..."
cmake --build "$BUILD_DIR" --config "$CONFIG" -j$(sysctl -n hw.ncpu 2>/dev/null || echo 4)

echo ""
echo "Build complete!"
echo ""
echo "Plugin artifacts are in: $BUILD_DIR/AnalyzerPro_artefacts/"
echo ""
echo "Built formats:"
if [ -d "$BUILD_DIR/AnalyzerPro_artefacts" ]; then
    find "$BUILD_DIR/AnalyzerPro_artefacts" -type f -name "*.component" -o -name "*.vst3" -o -name "*.app" 2>/dev/null | while read -r file; do
        echo "  - $file"
    done
fi
