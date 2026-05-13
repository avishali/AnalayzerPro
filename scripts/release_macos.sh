#!/usr/bin/env bash
# Full macOS release pipeline: Release build → PACE AAX (if present) → sign + notarize + archives.
#
# Prerequisites:
#   - JUCE_PATH: export in the shell, or set in scripts/.env (gitignored; see .env.example)
#   - Apple signing + notarytool profile (see scripts/sign_and_notarize.sh)
#   - For AAX: scripts/.aax_wraptool.env (see scripts/.aax_wraptool.env.example and docs/aax_pace.md)
#
# Canonical documentation: docs/release_macos.md

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
cd "$PROJECT_ROOT"

# shellcheck source=/dev/null
source "$SCRIPT_DIR/source_repo_env.sh"

BUILD_DIR="${ANALYZERPRO_RELEASE_BUILD_DIR:-build-release}"
PLUGIN_NAME="AnalyzerPro"
ARTIFACTS_DIR="$BUILD_DIR/${PLUGIN_NAME}_artefacts/Release"
AAX_MACHO="$ARTIFACTS_DIR/AAX/${PLUGIN_NAME}.aaxplugin/Contents/MacOS/${PLUGIN_NAME}"

echo "=========================================="
echo "  AnalyzerPro — full macOS release"
echo "=========================================="
echo "  BUILD_DIR: $BUILD_DIR"
echo "=========================================="
echo ""

if [[ -z "${JUCE_PATH:-}" ]]; then
  echo "❌ JUCE_PATH is not set. export JUCE_PATH=... or add it to scripts/.env" >&2
  exit 1
fi

echo "Step 1/3: Release build"
echo "-----------------------"
"$SCRIPT_DIR/build_release.sh"
echo ""

if [[ -f "$AAX_MACHO" ]]; then
  echo "Step 2/3: PACE sign AAX (wraptool)"
  echo "----------------------------------"
  "$SCRIPT_DIR/wraptool_sign_aax.sh" "$AAX_MACHO"
  echo ""
  export SIGN_AND_NOTARIZE_SKIP_AAX=1
else
  echo "Step 2/3: No AAX Mach-O at"
  echo "  $AAX_MACHO"
  echo "  (skipping wraptool; unset SIGN_AND_NOTARIZE_SKIP_AAX)"
  echo ""
  unset SIGN_AND_NOTARIZE_SKIP_AAX || true
fi

echo "Step 3/3: Apple sign + installer + notarize + zip/dmg"
echo "-----------------------------------------------------"
if [[ "${SIGN_AND_NOTARIZE_SKIP_AAX:-}" == "1" ]]; then
  echo "  (SIGN_AND_NOTARIZE_SKIP_AAX=1 — AAX will not be Apple re-signed)"
fi
"$SCRIPT_DIR/sign_and_notarize.sh"

echo ""
echo "=========================================="
echo "  Release pipeline finished"
echo "=========================================="
echo "  See installer/ for .pkg, signed .zip, signed .dmg"
echo "  Guide: docs/release_macos.md"
echo "=========================================="
