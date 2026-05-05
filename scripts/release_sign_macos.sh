#!/bin/bash

set -euo pipefail

# macOS release signing helper for AnalyzerPro v1.1.1 artifacts.
# This script signs Standalone/AU/VST3/AAX bundles in-place.

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
cd "$PROJECT_ROOT"

BUILD_DIR="${BUILD_DIR:-build-release-macos-universal}"
CONFIG="${CONFIG:-Release}"
PLUGIN_NAME="${PLUGIN_NAME:-AnalyzerPro}"
ARTIFACTS_DIR="${BUILD_DIR}/${PLUGIN_NAME}_artefacts/${CONFIG}"

IDENTITY="${DEVELOPER_ID_APP:-}"

if [[ -z "$IDENTITY" ]]; then
  echo "ERROR: DEVELOPER_ID_APP is not set."
  echo "Example:"
  echo "  export DEVELOPER_ID_APP='Developer ID Application: Your Name (TEAMID)'"
  exit 1
fi

if [[ ! -d "$ARTIFACTS_DIR" ]]; then
  echo "ERROR: Artifacts directory not found: $ARTIFACTS_DIR"
  echo "Run release build first:"
  echo "  cmake --preset release-macos-universal"
  echo "  cmake --build --preset release-macos-universal"
  exit 1
fi

sign_bundle() {
  local path="$1"
  if [[ -d "$path" ]]; then
    echo "Signing: $path"
    codesign --force --deep --options runtime --timestamp --sign "$IDENTITY" "$path"
    codesign --verify --deep --strict --verbose=2 "$path"
  else
    echo "Skip (not found): $path"
  fi
}

sign_bundle "${ARTIFACTS_DIR}/Standalone/${PLUGIN_NAME}.app"
sign_bundle "${ARTIFACTS_DIR}/AU/${PLUGIN_NAME}.component"
sign_bundle "${ARTIFACTS_DIR}/VST3/${PLUGIN_NAME}.vst3"
sign_bundle "${ARTIFACTS_DIR}/AAX/${PLUGIN_NAME}.aaxplugin"

echo ""
echo "Signing complete."
echo "Note: AAX still requires final PACE/Avid distribution signing workflow."
