# AnalyzerPro Release & Distribution Guide

Complete guide for building, signing, and distributing AnalyzerPro for macOS.

---

## Table of Contents

1. [Prerequisites](#prerequisites)
2. [Quick Start](#quick-start)
3. [Step-by-Step Release Process](#step-by-step-release-process)
4. [Troubleshooting](#troubleshooting)
5. [Distribution Checklist](#distribution-checklist)

---

## Prerequisites

### Required Software

- **macOS 10.13+** (for building)
- **Xcode Command Line Tools**
  ```bash
  xcode-select --install
  ```
- **CMake 3.22+**
  ```bash
  brew install cmake
  ```
- **JUCE Framework**
  - Set `JUCE_PATH` environment variable:
    ```bash
    export JUCE_PATH=/path/to/JUCE
    # Add to ~/.zshrc for persistence
    echo 'export JUCE_PATH=/path/to/JUCE' >> ~/.zshrc
    ```

### For Code Signing & Notarization (Required for Public Distribution)

- **Apple Developer Account** (paid)
- **Developer ID Application Certificate**
- **Developer ID Installer Certificate**
- **App-Specific Password** for notarization

---

## Quick Start

### For Testing/Development

```bash
# Build release version (unsigned)
./scripts/build_release.sh

# Create installer packages
./scripts/create_installer.sh
```

### For Public Distribution

```bash
# 1. Build release
./scripts/build_release.sh

# 2. Sign and notarize (requires Apple Developer credentials)
./scripts/sign_and_notarize.sh

# This creates signed, notarized packages ready for distribution
```

---

## Step-by-Step Release Process

### Step 1: Update Version Number

Edit `CMakeLists.txt` and update version:

```cmake
set(PLUGIN_VERSION_MAJOR 1 CACHE STRING "Plugin version major")
set(PLUGIN_VERSION_MINOR 0 CACHE STRING "Plugin version minor")
set(PLUGIN_VERSION_PATCH 0 CACHE STRING "Plugin version patch")
```

Also update in the scripts:
- `scripts/build_release.sh` → `PLUGIN_VERSION="1.0.0"`
- `scripts/create_installer.sh` → `PLUGIN_VERSION="1.0.0"`
- `scripts/sign_and_notarize.sh` → `PLUGIN_VERSION="1.0.0"`

### Step 2: Build Release Version

```bash
./scripts/build_release.sh
```

This script:
- ✅ Disables dev mode (enables LTO optimizations)
- ✅ Builds universal binaries (arm64 + x86_64)
- ✅ Builds all formats: AU, VST3, AAX, Standalone
- ✅ Uses Release configuration with maximum optimizations
- ✅ Sets minimum macOS target to 10.13

**Output location:**
```
build-release/AnalyzerPro_artefacts/Release/
├── AU/AnalyzerPro.component
├── VST3/AnalyzerPro.vst3
├── AAX/AnalyzerPro.aaxplugin
└── Standalone/AnalyzerPro.app
```

**Verify universal binaries:**
```bash
lipo -info build-release/AnalyzerPro_artefacts/Release/AU/AnalyzerPro.component/Contents/MacOS/AnalyzerPro
# Should show: Architectures in the fat file: x86_64 arm64
```

### Step 3: Test Built Plugins

Before creating installers, test thoroughly:

#### Test AU Plugin
```bash
# Copy to system location
cp -R build-release/AnalyzerPro_artefacts/Release/AU/AnalyzerPro.component \
      ~/Library/Audio/Plug-Ins/Components/

# Validate
auval -v aufx AnPr Melc
```

#### Test VST3 Plugin
```bash
# Copy to system location
cp -R build-release/AnalyzerPro_artefacts/Release/VST3/AnalyzerPro.vst3 \
      ~/Library/Audio/Plug-Ins/VST3/

# Test in your DAW
```

#### Test Standalone App
```bash
# Run directly
./build-release/AnalyzerPro_artefacts/Release/Standalone/AnalyzerPro.app/Contents/MacOS/AnalyzerPro

# Grant microphone permission when prompted
```

### Step 4: Create Installer Packages

```bash
./scripts/create_installer.sh
```

This script creates:
- ✅ **PKG installer** - Standard macOS package installer
- ✅ **ZIP archive** - Manual installation option
- ✅ **DMG disk image** - Drag-and-drop installation

**Output location:**
```
installer/
├── AnalyzerPro-1.0.0-macOS.pkg
├── AnalyzerPro-1.0.0-macOS.zip
└── AnalyzerPro-1.0.0-macOS.dmg
```

**What gets installed:**
```
~/Library/Audio/Plug-Ins/Components/AnalyzerPro.component  (AU)
~/Library/Audio/Plug-Ins/VST3/AnalyzerPro.vst3             (VST3)
/Applications/AnalyzerPro.app                               (Standalone)
```

### Step 5: Code Signing & Notarization (For Distribution)

⚠️ **Required for public distribution!** Unsigned plugins will be blocked by macOS Gatekeeper.

#### 5.1 Setup Apple Developer Credentials

1. **Obtain certificates** from Apple Developer Portal:
   - Developer ID Application Certificate
   - Developer ID Installer Certificate

2. **Create app-specific password:**
   - Go to https://appleid.apple.com
   - Sign in → Security → App-Specific Passwords
   - Generate new password

3. **Store credentials in keychain:**
   ```bash
   xcrun notarytool store-credentials AC_PASSWORD \
     --apple-id your@email.com \
     --team-id YOUR_TEAM_ID \
     --password your-app-specific-password
   ```

4. **Edit `scripts/sign_and_notarize.sh`:**
   ```bash
   DEVELOPER_ID_APP="Developer ID Application: Your Name (TEAM_ID)"
   DEVELOPER_ID_INSTALLER="Developer ID Installer: Your Name (TEAM_ID)"
   APPLE_ID="your@email.com"
   TEAM_ID="YOUR_TEAM_ID"
   ```

#### 5.2 Run Signing & Notarization

```bash
./scripts/sign_and_notarize.sh
```

This script:
1. ✅ Signs all plugin binaries with hardened runtime
2. ✅ Rebuilds installer with signed plugins
3. ✅ Signs the installer package
4. ✅ Submits to Apple for notarization (takes 5-15 minutes)
5. ✅ Staples notarization ticket
6. ✅ Creates final distribution archives

**Output:**
```
installer/
├── AnalyzerPro-1.0.0-macOS-signed.pkg  ✅ Ready for distribution
├── AnalyzerPro-1.0.0-macOS-signed.zip  ✅ Ready for distribution
└── AnalyzerPro-1.0.0-macOS-signed.dmg  ✅ Ready for distribution
```

#### 5.3 Verify Signing & Notarization

```bash
# Verify code signature
codesign --verify --deep --strict --verbose=2 \
  build-release/AnalyzerPro_artefacts/Release/AU/AnalyzerPro.component

# Check notarization
spctl --assess --verbose=4 --type install \
  installer/AnalyzerPro-1.0.0-macOS-signed.pkg

# Should output: "source=Notarized Developer ID"
```

### Step 6: AAX Signing (Pro Tools)

⚠️ AAX plugins require additional signing with PACE (Avid's protection system).

1. **Join AVID Developer Program**
   - https://my.avid.com/products/
   - Apply for AAX developer access

2. **Sign AAX with PACE iLok License Manager**
   - Use wraptool or Avid's signing process
   - This is separate from Apple code signing

3. **Test in Pro Tools**
   ```bash
   # Copy to Pro Tools plugin folder
   cp -R build-release/AnalyzerPro_artefacts/Release/AAX/AnalyzerPro.aaxplugin \
         /Library/Application\ Support/Avid/Audio/Plug-Ins/
   ```

---

## Troubleshooting

### Build Issues

#### "JUCE_PATH not set"
```bash
export JUCE_PATH=/path/to/JUCE
echo 'export JUCE_PATH=/path/to/JUCE' >> ~/.zshrc
```

#### "CMake configuration failed"
- Ensure CMake 3.22+ is installed: `cmake --version`
- Check JUCE path exists: `ls -la $JUCE_PATH`

#### "Build failed"
- Check for disk space: `df -h`
- Clean and rebuild:
  ```bash
  rm -rf build-release
  ./scripts/build_release.sh
  ```

### Signing Issues

#### "No signing certificate found"
```bash
# List available certificates
security find-identity -v -p codesigning

# Should show "Developer ID Application" certificate
```

#### "Notarization failed"
```bash
# Check notarization logs
xcrun notarytool log REQUEST_ID --keychain-profile AC_PASSWORD

# Common issues:
# - Hardened runtime not enabled (--options runtime)
# - Missing entitlements
# - Invalid signature
```

#### "Certificate expired"
- Renew certificates in Apple Developer Portal
- Download and install new certificates

### Installation Issues

#### "App is damaged and can't be opened"
- Plugin not properly signed/notarized
- User downloaded via Chrome (quarantine attribute)
  ```bash
  # Remove quarantine attribute
  xattr -dr com.apple.quarantine /path/to/plugin
  ```

#### "Plugin not showing in DAW"
- Verify installation location
- Check DAW plugin search paths
- Rescan plugins in DAW preferences
- Validate with `auval` (AU) or DAW's plugin validator

---

## Distribution Checklist

### Pre-Distribution Testing

- [ ] Test on **clean macOS system** (no dev tools)
- [ ] Test on **Intel Mac**
- [ ] Test on **Apple Silicon Mac**
- [ ] Test **AU** in Logic Pro / GarageBand
- [ ] Test **VST3** in Ableton Live / Reaper / FL Studio
- [ ] Test **AAX** in Pro Tools (if applicable)
- [ ] Test **Standalone** app
- [ ] Verify **microphone permission** prompt works
- [ ] Verify **Gatekeeper** doesn't block installation
- [ ] Test **uninstallation** process
- [ ] Verify **plugin presets** work correctly
- [ ] Test **state persistence** (close/reopen DAW project)

### Files to Distribute

Choose based on your distribution method:

#### Option 1: PKG Installer (Recommended)
```
✅ AnalyzerPro-1.0.0-macOS-signed.pkg
```
- Easiest for users (double-click to install)
- Standard macOS installation experience
- Requires notarization

#### Option 2: DMG Disk Image
```
✅ AnalyzerPro-1.0.0-macOS-signed.dmg
```
- Drag-and-drop installation
- Professional presentation
- Requires manual copying to plugin folders

#### Option 3: ZIP Archive
```
✅ AnalyzerPro-1.0.0-macOS-signed.zip
```
- Smallest file size
- Good for web downloads
- Requires manual installation instructions

### Documentation to Include

- [ ] **Installation guide**
- [ ] **System requirements**
- [ ] **Quick start guide**
- [ ] **License agreement**
- [ ] **Support contact information**
- [ ] **Changelog/release notes**

### Release Assets

Create a release package with:
```
AnalyzerPro-1.0.0-Release/
├── AnalyzerPro-1.0.0-macOS-signed.pkg
├── AnalyzerPro-1.0.0-macOS-signed.dmg
├── AnalyzerPro-1.0.0-macOS-signed.zip
├── README.md
├── LICENSE.txt
├── CHANGELOG.md
└── docs/
    ├── Installation_Guide.pdf
    └── User_Manual.pdf
```

### Distribution Platforms

Consider publishing on:
- [ ] Your website (direct download)
- [ ] Plugin Boutique
- [ ] Splice
- [ ] KVR Audio
- [ ] Gumroad
- [ ] App Store (requires additional Apple review)

---

## Version Control Best Practices

### Before Building Release

```bash
# Ensure clean working tree
git status

# Tag the release
git tag -a v1.0.0 -m "AnalyzerPro v1.0.0 Release"

# Push tag
git push origin v1.0.0
```

### After Successful Distribution

```bash
# Create release notes
git log v0.9.0..v1.0.0 --pretty=format:"- %s" > CHANGELOG.md

# Commit installer scripts and docs
git add scripts/build_release.sh scripts/create_installer.sh
git add docs/RELEASE_GUIDE.md
git commit -m "Add release build and distribution scripts"
git push
```

---

## Continuous Integration (Optional)

For automated builds, consider setting up GitHub Actions:

```yaml
# .github/workflows/release.yml
name: Build Release

on:
  push:
    tags:
      - 'v*'

jobs:
  build:
    runs-on: macos-latest
    steps:
      - uses: actions/checkout@v3
      - name: Setup JUCE
        run: |
          git clone https://github.com/juce-framework/JUCE.git
          echo "JUCE_PATH=$PWD/JUCE" >> $GITHUB_ENV
      
      - name: Build Release
        run: ./scripts/build_release.sh
      
      - name: Create Installer
        run: ./scripts/create_installer.sh
      
      - name: Upload Artifacts
        uses: actions/upload-artifact@v3
        with:
          name: AnalyzerPro-Release
          path: installer/
```

---

## Support & Resources

- **JUCE Documentation:** https://docs.juce.com
- **Apple Code Signing Guide:** https://developer.apple.com/documentation/security/notarizing_macos_software_before_distribution
- **AAX SDK:** https://my.avid.com/products/
- **macOS Gatekeeper:** https://support.apple.com/en-us/HT202491

---

## License & Copyright

```
AnalyzerPro v1.0.0
Copyright © 2026 MelecDSP. All rights reserved.
```

For commercial distribution, ensure:
- [ ] JUCE license compliance (GPL or Commercial)
- [ ] Third-party library licenses included
- [ ] Copyright notices in About dialog
- [ ] EULA/Terms of Service prepared

---

**Last Updated:** January 2026
