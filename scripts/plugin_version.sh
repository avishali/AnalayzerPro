#!/bin/bash
#
# Single source of truth for the plugin version used by all packaging/signing
# scripts. The canonical version lives in CMakeLists.txt:
#
#   set(PLUGIN_VERSION_MAJOR 1 ...)
#   set(PLUGIN_VERSION_MINOR 1 ...)
#   set(PLUGIN_VERSION_PATCH 1 ...)
#
# Source this file to get PLUGIN_VERSION exported, parsed directly from CMake so
# the scripts can never drift from the version stamped into the built binaries.
#
# Usage (from any script):
#   source "$(dirname "${BASH_SOURCE[0]}")/plugin_version.sh"
#   echo "$PLUGIN_VERSION"

_PV_SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
_PV_CMAKELISTS="$_PV_SCRIPT_DIR/../CMakeLists.txt"

if [ ! -f "$_PV_CMAKELISTS" ]; then
    echo "❌ plugin_version.sh: cannot find CMakeLists.txt at $_PV_CMAKELISTS" >&2
    return 1 2>/dev/null || exit 1
fi

_pv_extract() {
    # $1 = MAJOR|MINOR|PATCH
    grep -E "set\(PLUGIN_VERSION_$1[[:space:]]+[0-9]+" "$_PV_CMAKELISTS" \
        | head -n1 \
        | sed -E "s/.*set\(PLUGIN_VERSION_$1[[:space:]]+([0-9]+).*/\1/"
}

_PV_MAJOR="$(_pv_extract MAJOR)"
_PV_MINOR="$(_pv_extract MINOR)"
_PV_PATCH="$(_pv_extract PATCH)"

if [ -z "$_PV_MAJOR" ] || [ -z "$_PV_MINOR" ] || [ -z "$_PV_PATCH" ]; then
    echo "❌ plugin_version.sh: failed to parse PLUGIN_VERSION_* from CMakeLists.txt" >&2
    return 1 2>/dev/null || exit 1
fi

export PLUGIN_VERSION="${_PV_MAJOR}.${_PV_MINOR}.${_PV_PATCH}"
