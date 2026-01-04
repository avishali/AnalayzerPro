#!/bin/bash

# Clean build script for JUCE Plugin Template
# Removes build directories to force a clean rebuild

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

echo "Cleaning build directories..."

# Remove common build directory names
BUILD_DIRS=(
    "build"
    "build-template"
    "build-debug"
    "build-release"
    "cmake-build-*"
)

REMOVED=0

for dir in "${BUILD_DIRS[@]}"; do
    if [ -d "$dir" ]; then
        echo "  Removing $dir/"
        rm -rf "$dir"
        REMOVED=1
    fi
done

# Also remove any directory matching build-* pattern
for dir in build-*; do
    if [ -d "$dir" ] && [ "$dir" != "build-template" ]; then
        echo "  Removing $dir/"
        rm -rf "$dir"
        REMOVED=1
    fi
done

if [ $REMOVED -eq 0 ]; then
    echo "No build directories found to remove."
else
    echo "Build cleanup complete!"
fi
