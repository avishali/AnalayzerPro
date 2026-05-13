#!/bin/bash

# Release Build Script for AnalyzerPro
# Builds universal binaries (arm64 + x86_64) with optimizations for distribution

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
cd "$PROJECT_ROOT"

# shellcheck source=/dev/null
source "$SCRIPT_DIR/source_repo_env.sh"

BUILD_DIR="${ANALYZERPRO_RELEASE_BUILD_DIR:-build-release}"
CONFIG="Release"

# Plugin information
PLUGIN_NAME="AnalyzerPro"
PLUGIN_VERSION="1.0.0"
COMPANY_NAME="MelecDSP"

echo "=========================================="
echo "  AnalyzerPro Release Build"
echo "=========================================="
echo "  Version: $PLUGIN_VERSION"
echo "  Configuration: $CONFIG"
echo "  Universal Binary: arm64 + x86_64"
echo "=========================================="
echo ""

# Check if JUCE_PATH is set
if [ -z "$JUCE_PATH" ]; then
    echo "❌ Error: JUCE_PATH is not set."
    echo "  export JUCE_PATH=/path/to/JUCE"
    echo "  or add JUCE_PATH=... to scripts/.env (copy from scripts/.env.example)"
    exit 1
fi

# Verify JUCE path exists
if [ ! -d "$JUCE_PATH" ]; then
    echo "❌ Error: JUCE_PATH directory does not exist: $JUCE_PATH"
    exit 1
fi

echo "✓ JUCE path verified: $JUCE_PATH"
echo ""

# Clean previous release build
if [ -d "$BUILD_DIR" ]; then
    echo "🧹 Cleaning previous release build..."
    rm -rf "$BUILD_DIR"
fi

# Configure CMake for Release
echo "⚙️  Configuring CMake for Release build..."
echo ""
cmake -S "$PROJECT_ROOT" -B "$BUILD_DIR" \
    -DCMAKE_BUILD_TYPE="$CONFIG" \
    -DJUCE_PATH="$JUCE_PATH" \
    -DPLUGIN_DEV_MODE=OFF \
    -DUniversalBinary=ON \
    -DCMAKE_OSX_DEPLOYMENT_TARGET="10.13"

if [ $? -ne 0 ]; then
    echo "❌ CMake configuration failed!"
    exit 1
fi

echo ""
echo "✓ CMake configuration complete"
echo ""

# Build
echo "🔨 Building release binaries..."
echo "   This may take several minutes..."
echo ""

NUM_CORES=$(sysctl -n hw.ncpu 2>/dev/null || echo 4)
cmake --build "$BUILD_DIR" --config "$CONFIG" -j"$NUM_CORES"

if [ $? -ne 0 ]; then
    echo "❌ Build failed!"
    exit 1
fi

echo ""
echo "✅ Build complete!"
echo ""

# Display built artifacts
ARTIFACTS_DIR="$BUILD_DIR/${PLUGIN_NAME}_artefacts/$CONFIG"
echo "=========================================="
echo "  Built Artifacts"
echo "=========================================="
echo ""

if [ -d "$ARTIFACTS_DIR" ]; then
    # Check for AU
    if [ -d "$ARTIFACTS_DIR/AU/${PLUGIN_NAME}.component" ]; then
        echo "✓ AU (Audio Unit):"
        echo "  $ARTIFACTS_DIR/AU/${PLUGIN_NAME}.component"
        # Verify universal binary
        if command -v lipo &> /dev/null; then
            lipo -info "$ARTIFACTS_DIR/AU/${PLUGIN_NAME}.component/Contents/MacOS/${PLUGIN_NAME}" 2>/dev/null || true
        fi
        echo ""
    fi
    
    # Check for VST3
    if [ -d "$ARTIFACTS_DIR/VST3/${PLUGIN_NAME}.vst3" ]; then
        echo "✓ VST3:"
        echo "  $ARTIFACTS_DIR/VST3/${PLUGIN_NAME}.vst3"
        # Verify universal binary
        if command -v lipo &> /dev/null; then
            lipo -info "$ARTIFACTS_DIR/VST3/${PLUGIN_NAME}.vst3/Contents/MacOS/${PLUGIN_NAME}" 2>/dev/null || true
        fi
        echo ""
    fi
    
    # Check for AAX
    if [ -d "$ARTIFACTS_DIR/AAX/${PLUGIN_NAME}.aaxplugin" ]; then
        echo "✓ AAX:"
        echo "  $ARTIFACTS_DIR/AAX/${PLUGIN_NAME}.aaxplugin"
        echo ""
    fi
    
    # Check for Standalone
    if [ -d "$ARTIFACTS_DIR/Standalone/${PLUGIN_NAME}.app" ]; then
        echo "✓ Standalone:"
        echo "  $ARTIFACTS_DIR/Standalone/${PLUGIN_NAME}.app"
        # Verify universal binary
        if command -v lipo &> /dev/null; then
            lipo -info "$ARTIFACTS_DIR/Standalone/${PLUGIN_NAME}.app/Contents/MacOS/${PLUGIN_NAME}" 2>/dev/null || true
        fi
        echo ""
    fi
else
    echo "⚠️  Warning: Artifacts directory not found: $ARTIFACTS_DIR"
fi

echo "=========================================="
echo ""
echo "🎉 Release build successful!"
echo ""
echo "Next steps:"
echo "  1. Test all plugin formats"
echo "  2. From this build — sign & ship:"
echo "       AAX (if built): ./scripts/wraptool_sign_aax.sh \\"
echo "         $BUILD_DIR/${PLUGIN_NAME}_artefacts/Release/AAX/${PLUGIN_NAME}.aaxplugin/Contents/MacOS/${PLUGIN_NAME}"
echo "       Then: SIGN_AND_NOTARIZE_SKIP_AAX=1 ./scripts/sign_and_notarize.sh"
echo "       (omit SKIP_AAX if you did not PACE-sign AAX; see docs/release_macos.md)"
echo "  3. From clean tree next time — one script: ./scripts/release_macos.sh (build + AAX + sign)"
echo "  4. Unsigned .pkg/.zip/.dmg only: ./scripts/create_installer.sh"
echo ""
