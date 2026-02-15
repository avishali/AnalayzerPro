# Figma Setup Guide

Complete guide for organizing your Figma file for automated SVG asset export.

## File Structure Template

Create a Figma file with this recommended structure:

```
AnalyzerPro Assets (Page)
│
├── 📁 Icons (Frame) ────────────────────────────────────
│   │   Purpose: Simple, tintable UI controls
│   │   Size: 24×24px artboards
│   │   Color: Single color (will convert to currentColor)
│   │
│   ├── icon/chevron_down
│   ├── icon/chevron_up
│   ├── icon/chevron_left
│   ├── icon/chevron_right
│   ├── icon/close
│   ├── icon/play
│   ├── icon/pause
│   ├── icon/stop
│   ├── icon/settings
│   ├── icon/menu
│   └── icon/search
│
├── 📁 Illustrations (Frame) ────────────────────────────
│   │   Purpose: Complex, multi-color graphics
│   │   Size: Variable (match your design)
│   │   Color: Preserved (no tinting support)
│   │
│   ├── illustration/splash_screen
│   ├── illustration/empty_state
│   └── illustration/error_graphic
│
└── 📁 Mockups (Frame) ──────────────────────────────────
    │   Purpose: Design reference (exported as PNG)
    │   Size: Full component size
    │   Color: Full design with all states
    │
    ├── mockup/main_view_v2
    ├── mockup/control_panel
    └── mockup/settings_dialog
```

---

## Icon Frame Setup

### Step-by-Step Icon Creation

#### 1. Create Artboard
- Press `F` (Frame tool)
- Size: 24×24px (or consistent size for all icons)
- Background: Transparent (no fill)

#### 2. Name the Frame
- Format: `icon/name_in_snake_case`
- Examples:
  - `icon/chevron_down` ✅
  - `icon/play_button` ✅
  - `icon/chevronDown` ❌ (use snake_case)
  - `icon/play-button` ❌ (use underscore, not hyphen)

#### 3. Design the Icon
- **Single color:** Use white (#FFFFFF) or black (#000000)
- **Vector only:** No raster images or effects
- **Simple paths:** Fewer points = better performance

#### 4. Flatten Layers
- Select all layers in icon
- Right-click → Flatten
- OR: Use Boolean operations to combine paths

#### 5. Convert Strokes to Fills
- Select stroke path
- Object → Outline Stroke
- OR: Set stroke-width in Figma and flatten

#### 6. Final Check
- ✅ Frame is exactly 24×24px
- ✅ Frame name starts with `icon/`
- ✅ Only one color used
- ✅ All layers flattened
- ✅ No effects, shadows, or blurs
- ✅ Transparent background

---

## Icon Design Guidelines

### Size and Alignment

**Recommended Icon Size:**
```
Artboard: 24×24px
Icon padding: 2px on all sides
Active drawing area: 20×20px
```

**Alignment:**
- Center icon within artboard
- Use Auto Layout for consistent spacing
- Snap to pixel grid (View → Pixel Preview)

### Color Usage

**For Tintable Icons:**
```
✅ Single color (any hex value)
   Example: #FFFFFF (white)
   Result: Converted to currentColor

❌ Multiple colors
   Example: #FF0000 and #00FF00
   Result: Tinting won't work properly

❌ Gradients
   Example: Linear gradient
   Result: Cannot tint, colors preserved

❌ Opacity variations
   Example: Same color at 100% and 50%
   Result: May lose detail in conversion
```

**Processing Behavior:**
- Script detects single-color SVGs
- Automatically converts to `fill="currentColor"`
- Multi-color icons preserve colors (no tinting)

### Path Complexity

**Keep icons simple:**

| Metric | Recommended | Maximum |
|--------|-------------|---------|
| Path count | < 10 paths | 20 paths |
| Total points | < 100 points | 200 points |
| File size | < 2KB | 5KB |

**How to simplify:**
1. Select path
2. Right-click → Simplify
3. Or: Object → Path → Simplify (adjust slider)

### Supported SVG Features

**✅ Supported (JUCE compatible):**
- Basic shapes: path, rect, circle, ellipse
- Groups (`<g>`)
- Transforms: translate, rotate, scale
- Solid fills and strokes
- Simple gradients (may render differently)

**❌ Not Supported:**
- Animations (`<animate>`, `<animateTransform>`)
- Filters (blur, drop-shadow, etc.)
- External images or references
- Text (convert to outlines first)
- Masks (flatten before export)

---

## Illustration Setup

For complex, multi-color graphics:

### Frame Configuration

```
Name: illustration/your_name
Size: Variable (match your design needs)
Colors: Multiple colors allowed
Complexity: Higher complexity OK (will not tint)
```

### Design Guidelines

1. **Colors preserved:** Use your design system colors
2. **Optimize manually:** Reduce layers and effects in Figma
3. **Test rendering:** Complex SVGs may render differently in JUCE
4. **Consider PNG:** For very complex graphics, use mockup prefix

### Example Use Cases

- Splash screen graphics
- Empty state illustrations
- Onboarding graphics
- Feature highlights

---

## Mockup Setup

For design reference and documentation:

### Frame Configuration

```
Name: mockup/component_name
Export: PNG (not SVG)
Size: Full component size (e.g., 800×600px)
Purpose: Documentation and design review
```

### Guidelines

1. **Full designs:** Include all UI states
2. **Annotations:** Add design notes and specs
3. **Versions:** Use v1, v2 suffix for iterations
4. **Not embedded:** Mockups go to docs/design/, not in binary

---

## Figma API Access

### Getting Your Personal Access Token

1. **Open Figma Settings:**
   - Click your profile icon (top-right)
   - Settings

2. **Navigate to Tokens:**
   - Scroll to "Personal Access Tokens"

3. **Create Token:**
   - Click "Create new token"
   - Name: "AnalyzerPro Asset Export"
   - Click "Create"

4. **Copy Token:**
   - ⚠️ **Copy immediately!** You'll only see it once
   - Store in password manager
   - Add to `.env` file:
     ```bash
     FIGMA_TOKEN=figd_your_token_here
     ```

### Finding Your File ID

**From Figma URL:**
```
https://www.figma.com/design/FILE_ID_HERE/AnalyzerPro-Assets
                              ^^^^^^^^^^^^^^
                              This is your File ID
```

**Example:**
```
URL: https://www.figma.com/design/a1b2c3d4e5f6/My-Icons
File ID: a1b2c3d4e5f6
```

**Configuration:**
Edit `scripts/figma-export.config.json`:
```json
{
  "figmaFileId": "a1b2c3d4e5f6",
  ...
}
```

---

## Export Settings

### Recommended Figma Export Settings

**For Icons:**
```
Format: SVG
Include "id" attribute: ✅ Enabled
Outline text: ❌ Disabled (no text in icons)
Simplify stroke: ✅ Enabled
```

**For Illustrations:**
```
Format: SVG
Include "id" attribute: ✅ Enabled
Outline text: ✅ Enabled (if text present)
Simplify stroke: ❌ Disabled (preserve detail)
```

**For Mockups:**
```
Format: PNG
Scale: 2x (for retina displays)
```

### Script Export Settings

The export script uses these settings (configured in `figma-export.config.json`):

```json
{
  "assetTypes": {
    "icons": {
      "exportSettings": {
        "format": "SVG",
        "svgOutlineText": false,
        "svgSimplifyStroke": true
      }
    }
  }
}
```

---

## Naming Best Practices

### Icon Naming Convention

**Format:** `icon/descriptive_name`

**Good Examples:**
```
icon/chevron_down       ✅ Clear, descriptive
icon/play               ✅ Simple action
icon/settings_gear      ✅ Descriptive detail
icon/arrow_left         ✅ Direction specified
icon/search_magnify     ✅ Alternative name
```

**Bad Examples:**
```
chevron_down           ❌ Missing "icon/" prefix
icon/chevronDown       ❌ Use snake_case, not camelCase
icon/chevron-down      ❌ Use underscore, not hyphen
icon/1_chevron         ❌ Don't start with number
icon/chevron down      ❌ No spaces allowed
```

### Illustration Naming

**Format:** `illustration/descriptive_name`

**Examples:**
```
illustration/splash_screen
illustration/empty_state_music
illustration/error_404
```

### Mockup Naming

**Format:** `mockup/component_name_version`

**Examples:**
```
mockup/main_view_v1
mockup/main_view_v2
mockup/control_panel_dark
mockup/settings_modal
```

---

## Figma Components and Variants

### Using Figma Components

**Benefits:**
- Consistency across icons
- Easy updates (change component, update all instances)
- Maintain design system

**Workflow:**
1. Design base icon
2. Create component (Ctrl+Alt+K / Cmd+Opt+K)
3. Create instances for export frames
4. Name instances with `icon/` prefix

### Icon Variants

**For related icons:**

```
Icon Set: Chevrons
├── icon/chevron_up
├── icon/chevron_down
├── icon/chevron_left
└── icon/chevron_right
```

**Use Figma Variants:**
- Create component set
- Add direction variant
- Export each variant individually

---

## Testing and Validation

### Pre-Export Checklist

Before running export script:

1. **Frame Names:**
   - ✅ All icons start with `icon/`
   - ✅ Use snake_case naming
   - ✅ No spaces or special characters

2. **Icon Quality:**
   - ✅ Single color (for tintable icons)
   - ✅ Layers flattened
   - ✅ Strokes converted to fills
   - ✅ 24×24px artboard size

3. **File Organization:**
   - ✅ Icons in dedicated "Icons" frame
   - ✅ Illustrations in "Illustrations" frame
   - ✅ Mockups in "Mockups" frame

### Post-Export Validation

After running export:

```bash
# Check exported files
ls third_party/melechdsp-hq/shared/mdsp_ui/ui_assets/icons/

# Validate SVGs
npm run svg:validate

# Check export manifest
cat scripts/export-manifest.json | grep -A 5 "warnings"
```

---

## Collaboration

### Team Workflow

**For Design Teams:**

1. **Shared File:** Use single Figma file for all assets
2. **Branching:** Use Figma branches for experimental icons
3. **Review:** Use comments to approve icons before export
4. **Handoff:** Tag developer when icons are ready

**For Developers:**

1. **Read-Only Access:** Token only needs read permission
2. **Automated Sync:** CI/CD can run weekly exports
3. **Manual Trigger:** Run export script when notified

### Version Control

**In Figma:**
- Use Figma's version history
- Name versions: "Icon set v1", "Icon set v2"
- Document changes in version description

**In Git:**
- Commit processed SVGs to repo
- Include export manifest in commits
- Tag releases when adding icons

---

## Troubleshooting

### "Frame not found" during export

**Cause:** Frame name doesn't match prefix

**Solution:**
- Check frame name starts with `icon/`, `illustration/`, or `mockup/`
- Verify no typos in prefix
- Run with `--dry-run` to see what would be exported

### Icons exporting with wrong colors

**Cause:** Figma using instance colors

**Solution:**
- Detach instance before export
- Or: Use component main as export source

### File ID not working

**Cause:** Wrong file ID format

**Solution:**
- Copy file ID from browser URL
- Don't include the file name part
- Format: alphanumeric string only

---

## Examples

### Example Icon Frame (Figma)

```
Frame Name: icon/play
Size: 24×24px
Content:
  - Vector path (triangle pointing right)
  - Fill: #FFFFFF (white)
  - Centered in artboard
  - No stroke, effects, or shadows
```

### Example Exported SVG

```svg
<svg width="24" height="24" viewBox="0 0 24 24" xmlns="http://www.w3.org/2000/svg">
  <path fill="currentColor" d="M8 5v14l11-7z"/>
</svg>
```
_(After processing: fill="#FFFFFF" → fill="currentColor")_

---

## Summary

**Setup Checklist:**

- [ ] Figma file created with organized frames
- [ ] Icons use `icon/` prefix and 24×24px size
- [ ] Single color used for tintable icons
- [ ] Personal access token generated
- [ ] Token added to `.env` file
- [ ] File ID added to `figma-export.config.json`
- [ ] Dependencies installed (`npm install`)

**Ready to Export:**
```bash
npm run figma:export
```

**Need help?** See [FIGMA_SVG_WORKFLOW.md](./FIGMA_SVG_WORKFLOW.md) for complete workflow documentation.
