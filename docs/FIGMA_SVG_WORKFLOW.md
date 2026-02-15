# Figma SVG Asset Workflow

Complete guide for the automated Figma-to-C++ SVG asset pipeline.

## Overview

This workflow automates the export, processing, and integration of SVG assets from Figma into the AnalyzerPro JUCE plugin. It handles:

- **Automated export** from Figma using the REST API
- **SVG optimization** (file size reduction, cleanup)
- **Color conversion** for tinting support (`currentColor`)
- **JUCE compatibility validation**
- **Build system integration** (auto-generated `IconIds.generated.h`)

**Pipeline Flow:**
```
Figma Design File
    ↓ (npm run figma:export)
SVG Files in ui_assets/
    ↓ (npm run svg:process)
Optimized & Tintable SVGs
    ↓ (CMake build)
IconIds.generated.h + BinaryData
    ↓
C++ Code: ui_.icons().get(IconId::chevron_down)
```

---

## Quick Start

### Prerequisites

1. **Node.js 18+** installed
   ```bash
   node --version  # Should be >= 18.0.0
   ```

2. **Figma account** with access to your design file

3. **Personal Access Token** from Figma:
   - Go to [Figma Settings → Personal Access Tokens](https://www.figma.com/settings)
   - Click "Create new token"
   - Copy the token (you'll only see it once!)

### Initial Setup

1. **Install dependencies:**
   ```bash
   cd scripts
   npm install
   ```

2. **Configure environment:**
   ```bash
   # Copy example env file
   cp .env.example .env

   # Edit .env and add your Figma token
   echo "FIGMA_TOKEN=your_token_here" > .env
   ```

3. **Configure Figma file:**
   - Get your Figma file ID from the URL:
     ```
     https://www.figma.com/design/FILE_ID_HERE/...
                                ^^^^^^^^^^^^^^
     ```
   - Edit `scripts/figma-export.config.json`:
     ```json
     {
       "figmaFileId": "YOUR_FILE_ID_HERE",
       ...
     }
     ```

### Export and Process Assets

```bash
# 1. Export SVGs from Figma
npm run figma:export

# 2. Process SVGs (optimize, convert colors, validate)
npm run svg:process

# 3. Build project (generates IconIds and embeds assets)
cd ..
./scripts/build.sh
```

**Or run everything at once:**
```bash
npm run assets:sync && cd .. && ./scripts/build.sh
```

---

## Figma File Organization

### Naming Conventions

Assets are routed based on frame name prefixes:

| Prefix | Asset Type | Output Directory | Purpose |
|--------|-----------|------------------|---------|
| `icon/` | Icons | `ui_assets/icons/` | Simple, tintable UI controls |
| `illustration/` | Illustrations | `ui_assets/illustrations/` | Complex, multi-color graphics |
| `mockup/` | Mockups | `docs/design/mockups/` | Design references (PNG) |

### Example Figma Structure

```
AnalyzerPro Assets (Page)
├── Icons (Frame)
│   ├── icon/chevron_down      ← Will export as chevron_down.svg
│   ├── icon/chevron_up
│   ├── icon/play
│   ├── icon/stop
│   └── icon/settings
│
├── Illustrations (Frame)
│   ├── illustration/splash_screen
│   └── illustration/empty_state
│
└── Mockups (Frame)
    ├── mockup/main_view_v1
    └── mockup/control_panel
```

### Icon Design Guidelines

For best results with tinting:

1. **Size:** Use 24×24px artboards consistently
2. **Color:** Single color (will be converted to `currentColor`)
   - Use any color in Figma (typically white `#FFFFFF` or black `#000000`)
   - Avoid gradients, patterns, or multi-color icons
3. **Flatten layers:** Convert all strokes to fills
4. **Simplify:** Reduce path complexity before export
5. **Transparent background:** No background fill on the frame

**Example Good Icon:**
```svg
<svg width="24" height="24" viewBox="0 0 24 24">
  <path fill="currentColor" d="M12 2L2 7v6c0 5.55 3.84 10.74 9 12 5.16-1.26 9-6.45 9-12V7l-10-5z"/>
</svg>
```

**Example Bad Icon (multi-color):**
```svg
<svg width="24" height="24" viewBox="0 0 24 24">
  <path fill="#FF0000" d="..."/>  <!-- Red -->
  <path fill="#00FF00" d="..."/>  <!-- Green - won't tint properly! -->
</svg>
```

### Multi-Color Assets (Illustrations)

For complex graphics that need to preserve colors:

1. Use `illustration/` prefix
2. Colors will be preserved (no `currentColor` conversion)
3. Optimize manually in Figma before export
4. These cannot be tinted at runtime

---

## Using Assets in C++ Code

### Icons (Tintable)

```cpp
#include <mdsp_ui/IconCache.h>
#include <mdsp_ui/IconIds.generated.h>

// In your component:
void paint(juce::Graphics& g) override
{
    // Get icon from cache
    const auto& theme = ui_.theme();
    auto tintedIcon = ui_.icons().makeTinted(
        mdsp_ui::IconId::chevron_down,  // Icon ID
        theme.textPrimary,              // Tint color
        1.0f                            // Alpha
    );

    // Draw icon
    juce::Rectangle<float> iconBounds(10, 10, 24, 24);
    if (tintedIcon != nullptr) {
        tintedIcon->drawWithin(
            g,
            iconBounds,
            juce::RectanglePlacement::centred,
            1.0f
        );
    }
}
```

### Available Icon IDs

After building, check `third_party/melechdsp-hq/shared/mdsp_ui/include/mdsp_ui/IconIds.generated.h`:

```cpp
namespace mdsp_ui {
    enum class IconId {
        chevron_down,
        chevron_up,
        play,
        stop,
        settings,
        // ... all exported icons
    };
}
```

### Illustrations (Multi-Color)

**Note:** `IllustrationCache` is planned for future implementation. For now, complex illustrations should be handled as separate components or use the icon cache without tinting.

---

## npm Scripts Reference

| Command | Description |
|---------|-------------|
| `npm run figma:export` | Export all assets from Figma |
| `npm run svg:process` | Optimize and process SVGs |
| `npm run svg:validate` | Validate SVGs without modifying |
| `npm run assets:sync` | Full workflow (export + process) |

### Command Options

**Export Options:**
```bash
# Dry run (no files written)
npm run figma:export -- --dry-run

# Override file ID
npm run figma:export -- --file-id=YOUR_FILE_ID
```

**Process Options:**
```bash
# Validation only (no modifications)
npm run svg:validate

# Verbose output
npm run svg:process -- --verbose
```

---

## Build System Integration

### CMake Icon Generation

The build system automatically:

1. **Scans** `ui_assets/icons/` for SVG files
2. **Generates** `IconIds.generated.h` with enum values
3. **Embeds** SVG data using JUCE BinaryBuilder
4. **Links** to `mdsp_ui` library

**Incremental Builds:**
- Only regenerates when SVG files change
- Cached for fast rebuilds
- No manual intervention needed

### Build Errors

**"Icon directory not found":**
```
Run: npm run figma:export
```

**"No SVG files found":**
- Check Figma file has frames with `icon/` prefix
- Verify export completed successfully
- Check `third_party/melechdsp-hq/shared/mdsp_ui/ui_assets/icons/`

**"IconId not found at runtime":**
- Rebuild project to regenerate `IconIds.generated.h`
- Check that SVG file exists in icons directory
- Verify enum name matches file name (snake_case)

---

## Troubleshooting

### Common Issues

#### 1. Icon not loading in UI

**Symptoms:** `nullptr` returned from `icons().get(IconId)`

**Solutions:**
1. Check console for `IconCache` errors
2. Verify SVG file exists in `ui_assets/icons/`
3. Rebuild project to regenerate `IconIds.generated.h`
4. Check that icon name uses snake_case (e.g., `chevron_down` not `chevron-down`)

#### 2. Tinting not working

**Symptoms:** Icon displays but doesn't change color

**Possible causes:**
- SVG contains multiple colors
- SVG uses gradients or patterns
- `fill="currentColor"` not set

**Solutions:**
1. Check export manifest: look for `"tintable": false`
2. Simplify icon to single color in Figma
3. Re-export and re-process: `npm run assets:sync`
4. Manually inspect SVG file for color attributes

#### 3. Figma API rate limiting

**Symptoms:** "Rate limited" message during export

**Solutions:**
- Script automatically waits and retries
- Large exports may take several minutes
- Fallback: Export manually from Figma (File → Export)

#### 4. SVG too complex for JUCE

**Symptoms:** Build warnings or rendering issues

**Possible causes:**
- File size > 50KB
- Path count > 200
- Unsupported SVG elements (filters, animations)

**Solutions:**
1. Simplify icon in Figma:
   - Flatten layers
   - Reduce path points (Object → Path → Simplify)
   - Remove effects and filters
2. Use PNG fallback for complex assets
3. Check validation report: `npm run svg:validate`

#### 5. Build fails with missing IconIds

**Symptoms:** Compiler error about undefined `IconId::your_icon`

**Solutions:**
1. Run export script: `npm run figma:export`
2. Check `ui_assets/icons/` directory exists
3. Verify SVG files are present
4. Clean build: `rm -rf build && ./scripts/build.sh`

### Getting Help

**Export Manifest:**
Check `scripts/export-manifest.json` for detailed asset metadata:
- File paths
- Color counts
- Tintability flags
- Validation warnings

**Validation Report:**
Run `npm run svg:validate` for comprehensive validation without modifying files.

**Verbose Output:**
Add `--verbose` flag for detailed processing logs:
```bash
npm run svg:process -- --verbose
```

---

## Advanced Usage

### Manual Export Workflow

If Figma API is unavailable:

1. **Export from Figma:**
   - Select frames with `icon/` prefix
   - Right-click → Export
   - Format: SVG
   - Settings: Outline text OFF, Simplify stroke ON

2. **Place files:**
   ```bash
   # Move to icons directory
   mv ~/Downloads/*.svg third_party/melechdsp-hq/shared/mdsp_ui/ui_assets/icons/
   ```

3. **Process:**
   ```bash
   npm run svg:process
   ```

### Custom Asset Types

To add new asset categories, edit `scripts/figma-export.config.json`:

```json
{
  "assetTypes": {
    "logos": {
      "framePrefix": "logo/",
      "outputDir": "../assets/logos",
      "exportSettings": {
        "format": "SVG",
        "scale": 1
      },
      "preserveColors": true
    }
  }
}
```

### CI/CD Integration

See `.github/workflows/figma-assets.yml` for automated weekly asset syncs.

---

## Best Practices

### Figma Organization

1. **Single source of truth:** Keep one master Figma file for all assets
2. **Versioning:** Use frame names like `icon/play_v2` for iterations
3. **Components:** Use Figma components for consistency
4. **Variants:** Group related icons (e.g., play/pause/stop)

### Export Strategy

1. **Batch exports:** Export multiple icons at once
2. **Review manifest:** Check `export-manifest.json` for warnings
3. **Test locally:** Build and test before committing
4. **Version control:** Commit processed SVGs, not Figma exports directly

### Performance

1. **Keep icons simple:** Fewer paths = faster rendering
2. **Optimize before export:** Flatten in Figma first
3. **Batch processing:** Run `assets:sync` once for all changes
4. **Incremental builds:** CMake only rebuilds when SVGs change

---

## Related Documentation

- [FIGMA_SETUP.md](./FIGMA_SETUP.md) - Figma file setup guide
- [ASSET_TYPE_EXAMPLES.md](./ASSET_TYPE_EXAMPLES.md) - Visual examples
- [SVG_TROUBLESHOOTING.md](./SVG_TROUBLESHOOTING.md) - Detailed troubleshooting

---

## Summary

**Complete Workflow:**

```bash
# 1. Export from Figma
cd scripts
npm run figma:export

# 2. Process SVGs
npm run svg:process

# 3. Build project
cd ..
./scripts/build.sh

# 4. Use in code
# ui_.icons().get(IconId::your_icon)
```

**Key Points:**
- ✅ Name frames with `icon/` prefix in Figma
- ✅ Use single color for tintable icons
- ✅ Run `assets:sync` after Figma changes
- ✅ Rebuild project to update `IconIds.generated.h`
- ✅ Check manifest for warnings and errors

**Need help?** Check [SVG_TROUBLESHOOTING.md](./SVG_TROUBLESHOOTING.md) or review the export manifest for detailed diagnostics.
