# AnalyzerPro Release Quick Start

**Quick reference for building and distributing AnalyzerPro.**

---

## 🚀 Quick Commands

### Testing Build (Unsigned)
```bash
./scripts/build_release.sh
./scripts/create_installer.sh
```

### Production Build (Signed & Notarized)
```bash
./scripts/build_release.sh
./scripts/sign_and_notarize.sh  # Requires Apple Developer credentials
```

---

## 📋 Pre-Release Checklist

- [ ] Update version in `CMakeLists.txt`
- [ ] Update version in all scripts (build_release.sh, create_installer.sh, sign_and_notarize.sh)
- [ ] Set `JUCE_PATH` environment variable
- [ ] Test build on clean system
- [ ] Verify universal binaries (`lipo -info`)

---

## 🔑 Apple Developer Setup (First Time Only)

### 1. Store Notarization Credentials
```bash
xcrun notarytool store-credentials AC_PASSWORD \
  --apple-id your@email.com \
  --team-id YOUR_TEAM_ID \
  --password your-app-specific-password
```

### 2. Edit sign_and_notarize.sh
Update these variables:
```bash
DEVELOPER_ID_APP="Developer ID Application: Your Name (TEAM_ID)"
DEVELOPER_ID_INSTALLER="Developer ID Installer: Your Name (TEAM_ID)"
APPLE_ID="your@email.com"
TEAM_ID="YOUR_TEAM_ID"
```

---

## 📦 Output Files

### After `build_release.sh`:
```
build-release/AnalyzerPro_artefacts/Release/
├── AU/AnalyzerPro.component
├── VST3/AnalyzerPro.vst3
├── AAX/AnalyzerPro.aaxplugin
└── Standalone/AnalyzerPro.app
```

### After `create_installer.sh`:
```
installer/
├── AnalyzerPro-1.0.0-macOS.pkg  (unsigned)
├── AnalyzerPro-1.0.0-macOS.zip
└── AnalyzerPro-1.0.0-macOS.dmg
```

### After `sign_and_notarize.sh`:
```
installer/
├── AnalyzerPro-1.0.0-macOS-signed.pkg ✅ DISTRIBUTE THIS
├── AnalyzerPro-1.0.0-macOS-signed.zip ✅ DISTRIBUTE THIS
└── AnalyzerPro-1.0.0-macOS-signed.dmg ✅ DISTRIBUTE THIS
```

---

## ✅ Testing Checklist

### Before Distribution
- [ ] Install on clean macOS (no dev tools)
- [ ] Test on Intel Mac
- [ ] Test on Apple Silicon Mac
- [ ] Verify Gatekeeper doesn't block
- [ ] Test AU in Logic Pro / GarageBand
- [ ] Test VST3 in Ableton / Reaper
- [ ] Test AAX in Pro Tools
- [ ] Test Standalone app
- [ ] Verify microphone permission prompt
- [ ] Test plugin presets
- [ ] Test state persistence (save/load project)

---

## 🔧 Common Commands

### Verify Universal Binary
```bash
lipo -info path/to/AnalyzerPro.component/Contents/MacOS/AnalyzerPro
# Should show: x86_64 arm64
```

### Validate AU Plugin
```bash
auval -v aufx AnPr Melc
```

### Check Code Signature
```bash
codesign --verify --deep --strict --verbose=2 path/to/plugin
```

### Check Notarization
```bash
spctl --assess --verbose=4 --type install path/to/signed.pkg
# Should show: source=Notarized Developer ID
```

### Remove Quarantine (Testing Only)
```bash
xattr -dr com.apple.quarantine path/to/plugin
```

---

## 🐛 Quick Troubleshooting

| Problem | Solution |
|---------|----------|
| JUCE_PATH not set | `export JUCE_PATH=/path/to/JUCE` |
| Build fails | `rm -rf build-release && ./scripts/build_release.sh` |
| Signing fails | Check certificates: `security find-identity -v -p codesigning` |
| Notarization fails | Check logs: `xcrun notarytool log REQUEST_ID --keychain-profile AC_PASSWORD` |
| Plugin not in DAW | Check install location, rescan plugins |
| "App is damaged" | Plugin not signed/notarized properly |

---

## 📚 Full Documentation

For detailed instructions, see:
- **[docs/RELEASE_GUIDE.md](docs/RELEASE_GUIDE.md)** - Complete release process
- **[scripts/README.md](scripts/README.md)** - Script documentation

---

## 🎯 Distribution Options

### Choose One:

**PKG Installer (Recommended)**
- Easiest for users
- Standard macOS experience
- File: `AnalyzerPro-1.0.0-macOS-signed.pkg`

**DMG Disk Image**
- Professional presentation
- Drag-and-drop installation
- File: `AnalyzerPro-1.0.0-macOS-signed.dmg`

**ZIP Archive**
- Smallest file size
- Web-friendly
- File: `AnalyzerPro-1.0.0-macOS-signed.zip`

---

## ⚠️ Important Notes

1. **Always sign and notarize for public distribution!**
   - Unsigned plugins will be blocked by Gatekeeper
   
2. **AAX requires PACE signing**
   - Separate from Apple code signing
   - Apply to Avid Developer Program

3. **Test before distributing**
   - Clean system test is critical
   - Test both Intel and Apple Silicon

4. **Keep signed builds archived**
   - For customer support
   - For verification

---

## 📞 Support Resources

- **JUCE Docs:** https://docs.juce.com
- **Apple Notarization:** https://developer.apple.com/documentation/security/notarizing_macos_software_before_distribution
- **AAX SDK:** https://my.avid.com/products/

---

**Version:** 1.0.0  
**Company:** MelecDSP  
**Updated:** January 2026
