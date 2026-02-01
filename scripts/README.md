# Build & Release Scripts

This directory contains scripts for building, packaging, and distributing AnalyzerPro.

## Scripts Overview

### Development Scripts

#### `build.sh`
Standard development build script.

```bash
./scripts/build.sh [Release|Debug]
```

- Default: Release configuration
- Dev mode: ON (faster iteration, limited formats)
- Output: `build/AnalyzerPro_artefacts/`

---

### Release Scripts

#### `build_release.sh`
**Production release build with optimizations.**

```bash
./scripts/build_release.sh
```

Features:
- ✅ Dev mode: OFF (enables LTO optimizations)
- ✅ Universal binaries: arm64 + x86_64
- ✅ All formats: AU, VST3, AAX, Standalone
- ✅ Maximum optimization level
- ✅ macOS 10.13+ deployment target

Output: `build-release/AnalyzerPro_artefacts/Release/`

---

#### `create_installer.sh`
**Creates distribution packages.**

```bash
./scripts/create_installer.sh
```

Prerequisites:
- Run `build_release.sh` first

Creates:
- ✅ PKG installer (standard macOS installer)
- ✅ ZIP archive (manual installation)
- ✅ DMG disk image (drag-and-drop)

Output: `installer/`

Installation paths:
```
~/Library/Audio/Plug-Ins/Components/  (AU)
~/Library/Audio/Plug-Ins/VST3/        (VST3)
/Applications/                         (Standalone)
```

---

#### `sign_and_notarize.sh`
**Signs and notarizes for public distribution.**

```bash
./scripts/sign_and_notarize.sh
```

Prerequisites:
- Apple Developer Account
- Developer ID Application certificate
- Developer ID Installer certificate
- App-specific password for notarization
- Configured credentials (edit script first!)

Process:
1. Signs all plugin binaries with hardened runtime
2. Creates and signs installer package
3. Submits to Apple for notarization (~5-15 min)
4. Staples notarization ticket
5. Creates final distribution archives

Output:
```
installer/
├── AnalyzerPro-1.0.0-macOS-signed.pkg ✅
├── AnalyzerPro-1.0.0-macOS-signed.zip ✅
└── AnalyzerPro-1.0.0-macOS-signed.dmg ✅
```

**⚠️ Required for public distribution!** Unsigned plugins will be blocked by macOS Gatekeeper.

---

### Utility Scripts

#### `clean_build.sh`
Cleans all build artifacts.

```bash
./clean_build.sh
```

#### `run_standalone.sh`
Runs the standalone app directly.

```bash
./scripts/run_standalone.sh
```

---

## Quick Start Guide

### For Development
```bash
# Standard dev build
./scripts/build.sh

# Run standalone
./scripts/run_standalone.sh
```

### For Testing Release
```bash
# Build release version
./scripts/build_release.sh

# Create installers (unsigned)
./scripts/create_installer.sh

# Test PKG installer
open installer/AnalyzerPro-1.0.0-macOS.pkg
```

### For Public Distribution
```bash
# 1. Build release
./scripts/build_release.sh

# 2. Configure Apple credentials in sign_and_notarize.sh
nano scripts/sign_and_notarize.sh

# 3. Sign and notarize
./scripts/sign_and_notarize.sh

# Done! Distribution files are in installer/ directory
```

---

## Troubleshooting

### "JUCE_PATH not set"
```bash
export JUCE_PATH=/path/to/JUCE
echo 'export JUCE_PATH=/path/to/JUCE' >> ~/.zshrc
```

### Build fails
```bash
# Clean and rebuild
rm -rf build build-release
./scripts/build_release.sh
```

### Installer creation fails
```bash
# Ensure release build exists
ls -la build-release/AnalyzerPro_artefacts/Release/

# Rebuild if needed
./scripts/build_release.sh
```

### Signing fails
```bash
# Check certificates
security find-identity -v -p codesigning

# Should show "Developer ID Application" and "Developer ID Installer"
```

---

## Documentation

For detailed release and distribution guide, see:
**[docs/RELEASE_GUIDE.md](../docs/RELEASE_GUIDE.md)**

Covers:
- Complete release workflow
- Apple Developer setup
- Code signing & notarization
- Testing checklist
- Distribution options
- Troubleshooting

---

## Environment Variables

### Required
- `JUCE_PATH` - Path to JUCE framework installation

### Optional
- `CMAKE_BUILD_TYPE` - Override build configuration (Release/Debug)
- `CMAKE_OSX_ARCHITECTURES` - Override target architectures

---

## Build Outputs

### Development Build (`build/`)
```
build/
└── AnalyzerPro_artefacts/
    ├── Debug/ or Release/
        ├── AU/AnalyzerPro.component
        ├── VST3/AnalyzerPro.vst3
        └── Standalone/AnalyzerPro.app
```

### Release Build (`build-release/`)
```
build-release/
└── AnalyzerPro_artefacts/
    └── Release/
        ├── AU/AnalyzerPro.component      (Universal)
        ├── VST3/AnalyzerPro.vst3         (Universal)
        ├── AAX/AnalyzerPro.aaxplugin     (Universal)
        └── Standalone/AnalyzerPro.app    (Universal)
```

### Distribution Packages (`installer/`)
```
installer/
├── AnalyzerPro-1.0.0-macOS.pkg              (Unsigned)
├── AnalyzerPro-1.0.0-macOS.zip              (Unsigned)
├── AnalyzerPro-1.0.0-macOS.dmg              (Unsigned)
├── AnalyzerPro-1.0.0-macOS-signed.pkg       (Signed & Notarized) ✅
├── AnalyzerPro-1.0.0-macOS-signed.zip       (Signed & Notarized) ✅
└── AnalyzerPro-1.0.0-macOS-signed.dmg       (Signed & Notarized) ✅
```

---

## Script Maintenance

### Updating Version Numbers

When releasing a new version, update in:
1. `CMakeLists.txt` - Plugin version variables
2. `scripts/build_release.sh` - PLUGIN_VERSION
3. `scripts/create_installer.sh` - PLUGIN_VERSION
4. `scripts/sign_and_notarize.sh` - PLUGIN_VERSION

### Adding New Plugin Formats

Edit `CMakeLists.txt`:
```cmake
set(PLUGIN_FORMATS "AU;VST3;AAX;Standalone;LV2" ...)
```

Scripts will automatically handle new formats.

---

## Best Practices

1. **Always test unsigned builds first** before signing
2. **Test on clean macOS system** without dev tools
3. **Verify universal binaries** with `lipo -info`
4. **Validate AU plugins** with `auval`
5. **Keep credentials secure** (use keychain, never commit)
6. **Tag releases in git** (e.g., `git tag v1.0.0`)
7. **Archive signed builds** for future reference

---

## Support

For issues or questions:
- Check [docs/RELEASE_GUIDE.md](../docs/RELEASE_GUIDE.md)
- Review script output for specific error messages
- Verify prerequisites (JUCE_PATH, certificates, etc.)

---

**Scripts Version:** 1.0  
**Last Updated:** January 2026
