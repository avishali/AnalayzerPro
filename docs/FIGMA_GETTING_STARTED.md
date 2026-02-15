# Getting Started with Figma SVG Workflow

Quick start guide to set up and use the automated Figma SVG asset workflow.

## Prerequisites

- ✅ Node.js 18+ installed (`node --version`)
- ✅ Figma account with access to your design file
- ✅ 10 minutes of setup time

## Step 1: Get Figma Personal Access Token

1. **Open Figma in browser**
2. **Click profile icon** (top-right) → Settings
3. **Scroll to "Personal Access Tokens"**
4. **Click "Create new token"**
   - Name: "AnalyzerPro Asset Export"
   - Click "Create"
5. **Copy the token immediately** (you only see it once!)
   - Starts with `figd_`
   - Store in password manager

## Step 2: Find Your Figma File ID

1. **Open your Figma file** in browser
2. **Copy File ID from URL:**
   ```
   https://www.figma.com/design/FILE_ID_HERE/AnalyzerPro-Assets
                                ^^^^^^^^^^^^^^
                                This is your File ID
   ```
   Example: `a1b2c3d4e5f6g7h8`

## Step 3: Install and Configure

```bash
cd scripts

# Install Node.js dependencies
npm install

# Create environment file
cp .env.example .env

# Add your Figma token
echo "FIGMA_TOKEN=figd_your_token_here" > .env

# Configure Figma file ID
nano figma-export.config.json
# Change "figmaFileId": "YOUR_FIGMA_FILE_ID_HERE"
# to your actual file ID
```

## Step 4: Organize Your Figma File

Create frames with these naming conventions:

```
AnalyzerPro Assets (Page)
├── Icons (Frame)
│   ├── icon/play         ← Simple, tintable icons
│   ├── icon/pause
│   ├── icon/stop
│   └── icon/settings
│
└── Illustrations (Frame)
    └── illustration/splash_screen   ← Complex, multi-color graphics
```

**Icon Guidelines:**
- Size: 24×24px artboard
- Color: Single color (any color - will convert automatically)
- Flatten layers in Figma
- No effects, shadows, or gradients

## Step 5: First Export

```bash
# Test export (preview only, no files written)
npm run figma:export -- --dry-run

# If preview looks good, run actual export
npm run figma:export

# Process SVGs (optimize, convert colors, validate)
npm run svg:process
```

**Expected output:**
```
🎨 Figma Asset Exporter

📡 Fetching Figma file structure...
✓ Found 4 exportable assets

📦 Exporting 4 assets...
✓ Exported: play.svg (icons)
✓ Exported: pause.svg (icons)
✓ Exported: stop.svg (icons)
✓ Exported: settings.svg (icons)

📄 Manifest written to: export-manifest.json

📊 Export Summary
Total assets exported: 4
  icons: 4

✨ Export completed successfully!
```

## Step 6: Build and Use

```bash
# Go back to project root
cd ..

# Build project (generates IconIds and embeds assets)
./scripts/build.sh

# Icons are now available in C++!
```

## Step 7: Use in C++ Code

```cpp
#include <mdsp_ui/IconCache.h>
#include <mdsp_ui/IconIds.generated.h>

void paint(juce::Graphics& g) override
{
    // Get tinted icon
    auto icon = ui_.icons().makeTinted(
        mdsp_ui::IconId::play,      // Your icon from Figma!
        juce::Colours::red,         // Tint color
        1.0f                        // Alpha
    );

    // Draw it
    juce::Rectangle<float> bounds(10, 10, 24, 24);
    if (icon != nullptr) {
        icon->drawWithin(g, bounds,
                        juce::RectanglePlacement::centred,
                        1.0f);
    }
}
```

## Daily Workflow

**When you add/update icons in Figma:**

```bash
cd scripts

# Export and process
npm run assets:sync

# Build project
cd ..
./scripts/build.sh

# Done! New icons available in C++
```

**Quick commands:**
```bash
# Export from Figma
npm run figma:export

# Optimize SVGs
npm run svg:process

# Validate without modifying
npm run svg:validate

# Full workflow
npm run assets:sync
```

## Verify Setup

After first export, check these:

```bash
# 1. SVG files exported?
ls third_party/melechdsp-hq/shared/mdsp_ui/ui_assets/icons/
# Should see: play.svg, pause.svg, etc.

# 2. Export manifest created?
cat scripts/export-manifest.json | jq '.totalAssets'
# Should show: 4 (or however many icons you exported)

# 3. SVGs processed?
npm run svg:validate
# Should show: ✓ All SVG files passed validation!

# 4. IconIds generated after build?
grep "play" third_party/melechdsp-hq/shared/mdsp_ui/include/mdsp_ui/IconIds.generated.h
# Should show: play,
```

## Troubleshooting

### "FIGMA_TOKEN not set"
- Check `.env` file exists in `scripts/` directory
- Verify token starts with `figd_`
- No quotes needed around token

### "File not found (404)"
- Double-check File ID in `figma-export.config.json`
- Ensure you have access to the Figma file (open it in browser first)

### "No assets found"
- Verify frame names in Figma start with `icon/`
- Check that frames are not hidden
- Try dry-run: `npm run figma:export -- --dry-run`

### "Tinting not working"
- Check export manifest: `cat scripts/export-manifest.json | jq '.assets[0].metadata'`
- Should show: `"tintable": true`
- If false: Simplify icon to single color in Figma

### "Icon not found at runtime"
- Rebuild project: `./scripts/build.sh`
- Check generated header exists:
  ```bash
  ls third_party/melechdsp-hq/shared/mdsp_ui/include/mdsp_ui/IconIds.generated.h
  ```

## Next Steps

Now that you have the workflow set up:

1. **Read the full workflow guide:** [FIGMA_SVG_WORKFLOW.md](./FIGMA_SVG_WORKFLOW.md)
2. **Learn Figma setup best practices:** [FIGMA_SETUP.md](./FIGMA_SETUP.md)
3. **Bookmark troubleshooting guide:** [SVG_TROUBLESHOOTING.md](./SVG_TROUBLESHOOTING.md)

## Tips for Success

**In Figma:**
- ✅ Use consistent 24×24px size for icons
- ✅ Name frames with `icon/` prefix
- ✅ Keep icons simple (single color, few paths)
- ✅ Flatten layers before exporting

**In Terminal:**
- ✅ Always run `svg:process` after export
- ✅ Check manifest for warnings
- ✅ Rebuild after adding new icons
- ✅ Commit processed SVGs to git

**In Code:**
- ✅ Use `IconId` enum (auto-generated from file names)
- ✅ Tint icons with theme colors for consistency
- ✅ Check for `nullptr` before drawing
- ✅ Use `RectanglePlacement::centred` for best results

## Summary

**Complete first-time setup:**
```bash
# 1. Install
cd scripts && npm install

# 2. Configure
cp .env.example .env
# Edit .env and figma-export.config.json

# 3. Export
npm run figma:export

# 4. Process
npm run svg:process

# 5. Build
cd .. && ./scripts/build.sh
```

**Daily usage:**
```bash
cd scripts
npm run assets:sync
cd .. && ./scripts/build.sh
```

**That's it!** You now have a fully automated Figma-to-C++ SVG pipeline. 🎉

---

**Questions?** See [FIGMA_SVG_WORKFLOW.md](./FIGMA_SVG_WORKFLOW.md) or [SVG_TROUBLESHOOTING.md](./SVG_TROUBLESHOOTING.md)
