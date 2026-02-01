# Distributing Unsigned Builds (Testing & Private Use)

Guide for creating and distributing unsigned AnalyzerPro installers to other computers for testing or private use.

⚠️ **For Public Distribution:** Unsigned builds will be blocked by macOS Gatekeeper. For commercial distribution, see [RELEASE_GUIDE.md](RELEASE_GUIDE.md) for signing and notarization instructions.

---

## 🎯 Use Cases

Unsigned builds are suitable for:
- ✅ Beta testing with trusted testers
- ✅ Internal team distribution
- ✅ Personal use on multiple computers
- ✅ Development testing
- ❌ **NOT** for public/commercial distribution

---

## 📦 Creating Installers (On Build Computer)

### Prerequisites

- JUCE_PATH environment variable set
- macOS 10.13 or later
- Xcode Command Line Tools

### Build Commands

```bash
# 1. Navigate to project
cd /Users/avishaylidani/DEV/GitHubRepo/AnalyzerPro

# 2. Build release version
./scripts/build_release.sh

# 3. Create installer packages
./scripts/create_installer.sh
```

### Output Files

```
installer/
├── AnalyzerPro-1.0.0-macOS.pkg  (3-5 MB)  ← Best for most users
├── AnalyzerPro-1.0.0-macOS.zip  (2-4 MB)  ← Smallest file size
└── AnalyzerPro-1.0.0-macOS.dmg  (3-5 MB)  ← Professional presentation
```

**Choose based on your distribution method:**
- **PKG**: Easiest installation (double-click → done)
- **ZIP**: Best for email/web download (smallest)
- **DMG**: Best presentation, drag-and-drop installation

---

## 📤 Transferring to Other Computers

### Cloud Storage (Recommended)

**Dropbox / Google Drive / iCloud:**
1. Upload installer file
2. Share link with testers
3. They download and install

**Pros:** Easy, trackable, works anywhere  
**Cons:** Requires internet

### AirDrop (Local Network)

```bash
# On build computer, right-click file in Finder
# → Share → AirDrop → Select recipient Mac
```

**Pros:** Fast, no internet needed  
**Cons:** Requires proximity (same network)

### USB Drive / External Storage

```bash
# Copy to USB
cp installer/AnalyzerPro-1.0.0-macOS.pkg /Volumes/USB_DRIVE/

# On other computer
cp /Volumes/USB_DRIVE/AnalyzerPro-1.0.0-macOS.pkg ~/Downloads/
```

**Pros:** Reliable, works offline  
**Cons:** Physical transfer needed

### Email Attachment

**Only for small files (<10 MB):**
```bash
# Check file size first
ls -lh installer/AnalyzerPro-1.0.0-macOS.zip
```

**Pros:** Simple, direct  
**Cons:** File size limits, may be filtered

---

## 💻 Installation on Target Computer

### System Requirements

- macOS 10.13 or later
- Intel or Apple Silicon Mac
- No JUCE or development tools needed ✅
- No Xcode needed ✅

### Installation Methods

#### Method 1: PKG Installer (Easiest)

**Steps:**
1. Double-click `AnalyzerPro-1.0.0-macOS.pkg`
2. macOS Gatekeeper will block it (see bypass instructions below)
3. After bypassing, follow installer prompts
4. Restart DAW
5. Done!

**Installs to:**
```
~/Library/Audio/Plug-Ins/Components/AnalyzerPro.component  (AU)
~/Library/Audio/Plug-Ins/VST3/AnalyzerPro.vst3             (VST3)
/Applications/AnalyzerPro.app                               (Standalone)
```

#### Method 2: Manual Installation from ZIP

**Steps:**
1. Unzip `AnalyzerPro-1.0.0-macOS.zip`
2. Open Terminal
3. Run these commands:

```bash
# Navigate to unzipped folder
cd ~/Downloads  # or wherever you unzipped

# Copy AU
cp -R Library/Audio/Plug-Ins/Components/AnalyzerPro.component \
      ~/Library/Audio/Plug-Ins/Components/

# Copy VST3
cp -R Library/Audio/Plug-Ins/VST3/AnalyzerPro.vst3 \
      ~/Library/Audio/Plug-Ins/VST3/

# Copy Standalone
cp -R Applications/AnalyzerPro.app \
      /Applications/
```

4. Remove quarantine (see bypass instructions below)
5. Restart DAW

#### Method 3: DMG Disk Image

**Steps:**
1. Double-click `AnalyzerPro-1.0.0-macOS.dmg`
2. Read INSTALL.txt for instructions
3. Drag plugins to specified folders:
   - AU → `~/Library/Audio/Plug-Ins/Components/`
   - VST3 → `~/Library/Audio/Plug-Ins/VST3/`
   - App → `/Applications/`
4. Eject DMG
5. Remove quarantine (see bypass instructions below)
6. Restart DAW

---

## 🔓 Bypassing Gatekeeper (Required for Unsigned Builds)

macOS will block unsigned plugins. Users must explicitly allow them.

### For PKG Installer

**When blocked:**

1. **Double-click PKG** - macOS shows: *"AnalyzerPro-1.0.0-macOS.pkg cannot be opened"*

2. **Open System Settings:**
   - macOS 13+: System Settings → Privacy & Security
   - macOS 12 and earlier: System Preferences → Security & Privacy

3. **Click "Open Anyway":**
   - Scroll down to "Security" section
   - See message: *"AnalyzerPro-1.0.0-macOS.pkg was blocked from use"*
   - Click **"Open Anyway"** button
   - Confirm with password

4. **Try installer again** - Now it will open

### For Individual Plugins/App

**Method 1: Right-Click Method** (Easiest)

1. **Locate plugin** in Finder:
   ```
   ~/Library/Audio/Plug-Ins/Components/AnalyzerPro.component
   ~/Library/Audio/Plug-Ins/VST3/AnalyzerPro.vst3
   /Applications/AnalyzerPro.app
   ```

2. **Right-click** (or Control-click) the file

3. **Select "Open"** from menu

4. **Click "Open"** in the security dialog

5. **Repeat for each plugin** (AU, VST3, Standalone)

**Method 2: System Settings** (After First Block)

1. Try to open/use the plugin (gets blocked)
2. Go to System Settings → Privacy & Security
3. Scroll down to see blocked item
4. Click "Open Anyway"
5. Repeat for each plugin

**Method 3: Command Line** (Advanced Users)

```bash
# Remove quarantine attribute from all plugins
xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/Components/AnalyzerPro.component
xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/VST3/AnalyzerPro.vst3
xattr -dr com.apple.quarantine /Applications/AnalyzerPro.app

# Verify removal
xattr -l ~/Library/Audio/Plug-Ins/Components/AnalyzerPro.component
# Should show: (no output if successful)
```

---

## ✅ Verification After Installation

### Check Installation

```bash
# Verify AU
ls -la ~/Library/Audio/Plug-Ins/Components/AnalyzerPro.component

# Verify VST3
ls -la ~/Library/Audio/Plug-Ins/VST3/AnalyzerPro.vst3

# Verify Standalone
ls -la /Applications/AnalyzerPro.app
```

### Validate AU Plugin

```bash
# Run AU validation
auval -v aufx AnPr Melc

# Should output: "PASS" at the end
```

### Test in DAW

1. **Restart your DAW** (if it was open)
2. **Rescan plugins** (if needed)
3. **Create new track/channel**
4. **Load AnalyzerPro** from plugin list
5. **Test basic functionality**

### Test Standalone App

```bash
# Launch from Applications
open /Applications/AnalyzerPro.app

# Or from command line
/Applications/AnalyzerPro.app/Contents/MacOS/AnalyzerPro
```

**First launch:**
- Grant microphone permission when prompted
- This allows audio input from devices and virtual loopback (BlackHole, etc.)

---

## 📝 Distribution Instructions Template

When sending to testers, include these instructions:

```
AnalyzerPro v1.0.0 - Beta Test Build

INSTALLATION:
1. Download: AnalyzerPro-1.0.0-macOS.pkg
2. Double-click to install
3. When blocked by macOS:
   - Go to System Settings → Privacy & Security
   - Click "Open Anyway"
   - Run installer again
4. Restart your DAW
5. Load AnalyzerPro plugin

IMPORTANT: This is an unsigned beta build. macOS will warn you 
because it's not notarized. This is normal for test builds.

TROUBLESHOOTING:
- Plugin not showing? Rescan plugins in your DAW
- Still blocked? Right-click plugin file → Open
- Need help? Contact: your@email.com

SYSTEM REQUIREMENTS:
- macOS 10.13 or later
- Intel or Apple Silicon Mac
- Compatible DAW (Logic, Ableton, Reaper, Pro Tools, etc.)
```

---

## 🐛 Common Issues & Solutions

### "Plugin not found in DAW"

**Solutions:**
1. Verify installation location (see above)
2. Restart DAW completely (Quit → Reopen)
3. Rescan plugins in DAW preferences
4. Check DAW plugin search paths include standard locations

### "Plugin crashes on load"

**Solutions:**
1. Verify universal binary on Apple Silicon:
   ```bash
   lipo -info ~/Library/Audio/Plug-Ins/VST3/AnalyzerPro.vst3/Contents/MacOS/AnalyzerPro
   # Should show: x86_64 arm64
   ```
2. Check Console.app for crash logs
3. Try removing and reinstalling
4. Test standalone app first to isolate issue

### "Microphone permission not requested" (Standalone)

**Solution:**
1. Open System Settings → Privacy & Security → Microphone
2. Manually enable for AnalyzerPro
3. Restart app

### "Different Macs, same error"

**Common causes:**
- File corrupted during transfer (verify file size/checksum)
- macOS version too old (need 10.13+)
- Plugin installed to wrong location
- Quarantine not removed properly

---

## 📊 Build Information Checklist

Include with distribution for support:

```
AnalyzerPro Build Info
======================
Version: 1.0.0
Build Date: [Date]
Build Configuration: Release (Universal)
Architectures: x86_64, arm64
Minimum macOS: 10.13

Formats Included:
- AU (Audio Unit)
- VST3
- AAX (unsigned - requires PACE signing for Pro Tools)
- Standalone

Known Issues:
- [List any known issues]

Build Hash: [Optional - git commit hash]
```

---

## 🔄 Updating to New Version

### For Users

1. Delete old version first (optional but recommended):
   ```bash
   rm -rf ~/Library/Audio/Plug-Ins/Components/AnalyzerPro.component
   rm -rf ~/Library/Audio/Plug-Ins/VST3/AnalyzerPro.vst3
   rm -rf /Applications/AnalyzerPro.app
   ```

2. Install new version using same method as initial install

3. Restart DAW

**Note:** Plugin presets/settings are typically preserved (stored separately)

---

## 🚀 When Ready for Public Distribution

Unsigned builds are **not suitable** for public distribution. When ready:

1. **Get Apple Developer Account** ($99/year)
2. **Obtain code signing certificates**
3. **Follow signed distribution process**: [RELEASE_GUIDE.md](RELEASE_GUIDE.md)
4. **Use `sign_and_notarize.sh` script**

Signed builds:
- ✅ No Gatekeeper warnings
- ✅ Professional appearance
- ✅ Required for Mac App Store
- ✅ Required for most commercial distribution platforms

---

## 📧 Support Template for Testers

```
Hi [Tester],

Thanks for testing AnalyzerPro!

Download: [Link to installer]
Installation Guide: [Link to this doc or simplified version]

If you encounter issues:
1. Check installation location (see guide)
2. Remove quarantine: xattr -dr com.apple.quarantine [path]
3. Contact me with:
   - macOS version
   - DAW name and version
   - Error message/screenshot
   - Console logs (if crash)

Best regards,
[Your Name]
```

---

## 🔒 Security Notes

**For Testers:**
- Only install plugins from trusted developers
- Verify file source before installation
- Check file integrity if available (checksums)

**For Developers:**
- Clearly mark as "beta" or "test" build
- Only distribute to known/trusted testers
- Don't publish unsigned builds publicly
- Transition to signed builds for wider distribution

---

## 📚 Additional Resources

- **Main Release Guide:** [RELEASE_GUIDE.md](RELEASE_GUIDE.md)
- **Quick Start:** [../RELEASE_QUICKSTART.md](../RELEASE_QUICKSTART.md)
- **Script Documentation:** [../scripts/README.md](../scripts/README.md)

---

**Last Updated:** January 2026  
**For:** AnalyzerPro v1.0.0
