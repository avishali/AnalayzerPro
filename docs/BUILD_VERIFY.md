# Build Verification SOP

This document provides a repeatable procedure to verify a clean build with zero warnings.

## Prerequisites

- JUCE SDK installed at `/Users/avishaylidani/DEV/SDK/JUCE`
- CMake 3.15+
- Ninja build system

## Repo Cleanliness Check

Before building, verify the repository is clean:

```bash
# Main repo
git status
git submodule status

# Submodule cleanliness
git -C third_party/melechdsp-hq status
```

**Expected:** Clean working tree everywhere (no uncommitted changes).

## Clean Build Procedure

```bash
# Set JUCE path
export JUCE_PATH=/Users/avishaylidani/DEV/SDK/JUCE

# Clean previous build
rm -rf build-ninja

# Configure
cmake -S . -B build-ninja -G Ninja -DCMAKE_BUILD_TYPE=Debug

# Build and capture output
cmake --build build-ninja -j 2>&1 | tee /tmp/build.log
```

## Zero-Warnings Gate

After building, verify zero warnings:

```bash
if grep -q "warning:" /tmp/build.log; then
  echo "FAIL: warnings present"
  grep -n "warning:" /tmp/build.log | sed -n '1,200p'
  exit 1
else
  echo "OK: zero warnings"
fi
```

**Expected:** `OK: zero warnings`

## Quick Verification Script

For convenience, use the provided script:

```bash
./scripts/check-no-warnings.sh
```

This script performs the build and zero-warnings check automatically.
