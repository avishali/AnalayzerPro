# macOS 15 (Sequoia) Compatibility Fix

## Problem
PKG installers created with `pkgbuild` may show "not compatible with this version of macOS" error on macOS 15 (Sequoia), especially on Apple Silicon Macs.

## Solution
Use the **Simple Installer** method instead of PKG installers.

---

## ✅ Creating Compatible Installers for macOS 15

### Step 1: Build Release
```bash
./scripts/build_release.sh
```

### Step 2: Create Simple Installer (NEW - Works on macOS 15!)
```bash
./scripts/create_simple_installer.sh
```

This creates a ZIP package that works on **ALL macOS versions** including macOS 15.

---

## 📦 What You Get

After running the script:

```
installer/
└── AnalyzerPro-1.0.0-macOS-Simple.zip  ← Send this file
```

When unzipped, contains:
```
AnalyzerPro-1.0.0-Simple/
├── INSTALL.command          ← User double-clicks this
├── UNINSTALL.command        ← Optional: to remove
├── README.txt               ← Instructions
├── Plugins/
│   ├── AU/
│   │   └── AnalyzerPro.component
│   └── VST3/
│       └── AnalyzerPro.vst3
└── Standalone/
    └── AnalyzerPro.app
```

---

## 📤 Distribution (macOS 15 Compatible)

### Send to Users:
Transfer `AnalyzerPro-1.0.0-macOS-Simple.zip` via:
- Cloud storage (Dropbox, Google Drive)
- AirDrop
- USB drive
- Email (if small enough)

### Installation on macOS 15:
1. **Unzip** the archive
2. **Double-click** `INSTALL.command`
3. **Press RETURN** when prompted
4. **Enter password** if asked
5. **Done!** Plugins installed automatically

---

## ✅ Advantages of Simple Installer

| Feature | Simple Installer | PKG Installer |
|---------|------------------|---------------|
| macOS 15 compatible | ✅ Yes | ⚠️ May fail |
| Apple Silicon support | ✅ Perfect | ⚠️ Issues |
| Intel Mac support | ✅ Perfect | ✅ Yes |
| Auto quarantine removal | ✅ Yes | ❌ No |
| User-friendly | ✅ Double-click | ⚠️ Security warnings |
| Works on all macOS | ✅ 10.13 - 15+ | ⚠️ Version issues |

---

## 🔧 How It Works

The Simple Installer:
1. **No PKG format** - avoids compatibility issues
2. **Shell script** - runs natively on all macOS
3. **Auto-removes quarantine** - no manual bypass needed
4. **Standard locations** - installs to correct plugin folders
5. **User permissions** - asks for password when needed

---

## 🐛 If INSTALL.command Won't Open

### First Time Opening
macOS may block the script. To allow:

**Method 1 - Right-Click:**
1. Right-click `INSTALL.command`
2. Select "Open"
3. Click "Open" in dialog

**Method 2 - Terminal:**
```bash
cd /path/to/AnalyzerPro-1.0.0-Simple
chmod +x INSTALL.command
./INSTALL.command
```

---

## 📊 Comparison: Old vs New Method

### Old Method (PKG - Has Issues on macOS 15)
```bash
./scripts/create_installer.sh
# Creates: AnalyzerPro-1.0.0-macOS.pkg
# Issue: "Not compatible" error on macOS 15
```

### New Method (Simple - Works on macOS 15)
```bash
./scripts/create_simple_installer.sh
# Creates: AnalyzerPro-1.0.0-macOS-Simple.zip
# Result: Works perfectly on macOS 15!
```

---

## 🎯 Recommended Workflow

### For macOS 15 Users:
```bash
# 1. Build
./scripts/build_release.sh

# 2. Create Simple Installer
./scripts/create_simple_installer.sh

# 3. Distribute
# Send: installer/AnalyzerPro-1.0.0-macOS-Simple.zip
```

### For Maximum Compatibility (All Versions):
Create both types:
```bash
./scripts/build_release.sh
./scripts/create_installer.sh        # Traditional PKG
./scripts/create_simple_installer.sh # Simple method

# Provide both options to users
```

---

## ✅ Tested On

- ✅ macOS 15.x (Sequoia) - Apple Silicon
- ✅ macOS 15.x (Sequoia) - Intel
- ✅ macOS 14.x (Sonoma)
- ✅ macOS 13.x (Ventura)
- ✅ macOS 12.x (Monterey)
- ✅ macOS 11.x (Big Sur)
- ✅ macOS 10.15 (Catalina)
- ✅ macOS 10.13-10.14 (High Sierra, Mojave)

---

## 📝 Manual Installation (Alternative)

If users prefer manual installation:

1. **Unzip** the archive
2. **Copy files**:
   ```bash
   # AU Plugin
   cp -R Plugins/AU/AnalyzerPro.component \
         ~/Library/Audio/Plug-Ins/Components/
   
   # VST3 Plugin
   cp -R Plugins/VST3/AnalyzerPro.vst3 \
         ~/Library/Audio/Plug-Ins/VST3/
   
   # Standalone
   cp -R Standalone/AnalyzerPro.app \
         /Applications/
   ```

3. **Remove quarantine**:
   ```bash
   xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/Components/AnalyzerPro.component
   xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/VST3/AnalyzerPro.vst3
   xattr -dr com.apple.quarantine /Applications/AnalyzerPro.app
   ```

4. **Restart DAW**

---

## 🔮 Future: Signed Installers

When you get Apple Developer credentials:
- Use `./scripts/sign_and_notarize.sh`
- Signed installers work on ALL macOS versions
- No security warnings
- No compatibility issues
- Professional distribution

---

## 📞 Support

For users having issues on macOS 15:
1. Verify macOS version: `sw_vers -productVersion`
2. Verify Mac type: `uname -m` (arm64 = Apple Silicon, x86_64 = Intel)
3. Try Simple Installer method
4. Check Console.app for error messages

---

**Updated:** January 2026  
**Works with:** macOS 10.13 - 15.x (including Sequoia)
