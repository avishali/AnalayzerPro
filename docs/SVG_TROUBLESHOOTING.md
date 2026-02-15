# SVG Troubleshooting Guide

Comprehensive troubleshooting for the Figma SVG asset workflow.

## Table of Contents

1. [Folder structure and naming](#folder-structure-and-naming)
2. [Export Issues](#export-issues)
3. [Processing Issues](#processing-issues)
4. [Build Issues](#build-issues)
5. [Runtime Issues](#runtime-issues)
6. [Tinting Issues](#tinting-issues)
7. [Performance Issues](#performance-issues)
8. [Advanced Debugging](#advanced-debugging)

---

## Folder structure and naming

The export script and build expect **exact paths** from the AnalyzerPro repo root. If your folders are named or placed differently, exports or CMake will fail.

### Expected paths (from repo root)

| Asset type   | Expected path |
|-------------|----------------|
| Icons       | `third_party/melechdsp-hq/shared/mdsp_ui/ui_assets/icons/` |
| Illustrations | `third_party/melechdsp-hq/shared/mdsp_ui/ui_assets/illustrations/` |
| Mockups     | `docs/design/mockups/` |

Important: the shared UI library lives under **`mdsp_ui`** and assets under **`ui_assets`** (not `Assets`, `assets`, or `mdsp_ui/assets`).

### How the script resolves paths

- `figma-export.js` runs from the **`scripts/`** directory.
- In `figma-export.config.json`, each `outputDir` is **relative to `scripts/`**.
- Example: `"outputDir": "../third_party/melechdsp-hq/shared/mdsp_ui/ui_assets/icons"` resolves to the path above when run from `scripts/`.

### If your folder naming is different

**1. Verify what you actually have:**

```bash
# From repo root
ls -la third_party/melechdsp-hq/shared/mdsp_ui/
# You should see: ui_assets/ (and e.g. include/, src/, ...)

ls -la third_party/melechdsp-hq/shared/mdsp_ui/ui_assets/
# You should see: icons/  (and optionally illustrations/, scripts/, ...)
```

If you see different names (e.g. `Assets`, `icons` in another place), either rename folders to match the table above **or** change the config (and any CMake that points at icons) to match your layout.

**2. Align the export config with your layout:**

Edit `scripts/figma-export.config.json` and set each `outputDir` to a path **relative to `scripts/`** that matches your real folders. For the default layout:

```json
"assetTypes": {
  "icons": {
    "outputDir": "../third_party/melechdsp-hq/shared/mdsp_ui/ui_assets/icons",
    ...
  },
  "illustrations": {
    "outputDir": "../third_party/melechdsp-hq/shared/mdsp_ui/ui_assets/illustrations",
    ...
  },
  "mockups": {
    "outputDir": "../docs/design/mockups",
    ...
  }
}
```

If your assets live elsewhere (e.g. `shared/mdsp_ui/Assets/icons`), change only the path strings; keep running the script from `scripts/`.

**3. Figma frame naming (separate from disk folders):**

Figma **frame names** (prefixes) are independent of disk paths. The script decides *type* from the prefix and then writes to the `outputDir` for that type:

- Frame name starts with `icon/` → file goes to the icons `outputDir`.
- Frame name starts with `illustration/` → file goes to the illustrations `outputDir`.
- Frame name starts with `mockup/` → file goes to the mockups `outputDir`.

For **icons**, if `preserveSubfolders` is `true` in config, slashes in the frame name become subfolders: e.g. `icon/MDSP/Toggle/Pill/M` → `icons/MDSP/Toggle/Pill/M.svg` (each path segment gets the same naming convention, e.g. snake_case). If the name has no slash, the file is written directly under `outputDir`.

So: **folder naming** = disk paths above; **frame naming** = `icon/`, `illustration/`, `mockup/` in Figma.

---

## Export Issues

### No assets exported from Figma

**Symptoms:**
```
✓ Found 0 exportable assets
⚠️  No assets found. Check your naming conventions in Figma
```

**Diagnosis:**
1. Verify frame names in Figma start with correct prefix:
   - `icon/` for icons
   - `illustration/` for illustrations
   - `mockup/` for mockups

2. Check export config matches Figma structure:
   ```bash
   cat scripts/figma-export.config.json | grep framePrefix
   ```

**Solution:**
```bash
# Rename frames in Figma to include prefix
icon/your_icon_name

# Or update framePrefix in config.json
{
  "assetTypes": {
    "icons": {
      "framePrefix": "icon/"  # Must match Figma names
    }
  }
}
```

---

### Figma API authentication failed

**Symptoms:**
```
❌ Figma API error (401): Invalid token
```

**Diagnosis:**
```bash
# Check if token is set
cat scripts/.env | grep FIGMA_TOKEN

# Check if token format is correct (starts with figd_)
echo $FIGMA_TOKEN
```

**Solution:**
1. Generate new token at https://www.figma.com/settings
2. Update `.env` file:
   ```bash
   echo "FIGMA_TOKEN=figd_your_new_token" > scripts/.env
   ```
3. Retry export:
   ```bash
   npm run figma:export
   ```

---

### Rate limit exceeded

**Symptoms:**
```
⚠️  Rate limited. Waiting 60s...
```

**Diagnosis:**
- Figma API limit: 500 requests/minute
- Large exports trigger rate limiting

**Solution:**
- Script automatically waits and retries
- For very large exports, run overnight
- Or export in batches:
  ```bash
  # Export only specific asset type
  # (Modify config temporarily to comment out other types)
  npm run figma:export
  ```

---

### File not found (404)

**Symptoms:**
```
❌ Figma API error (404): File not found
```

**Diagnosis:**
```bash
# Check file ID in config
cat scripts/figma-export.config.json | grep figmaFileId
```

**Solution:**
1. Get correct file ID from Figma URL:
   ```
   https://www.figma.com/design/FILE_ID/...
                                ^^^^^^^^
   ```

2. Update `figma-export.config.json`:
   ```json
   {
     "figmaFileId": "YOUR_CORRECT_FILE_ID"
   }
   ```

3. Verify access: Open Figma file in browser while logged in

---

### Export corrupted or empty SVG files

**Symptoms:**
- SVG files exported but empty
- File size: 0 bytes

**Diagnosis:**
```bash
# Check exported files
ls -lh third_party/melechdsp-hq/shared/mdsp_ui/ui_assets/icons/

# Inspect file content
cat third_party/melechdsp-hq/shared/mdsp_ui/ui_assets/icons/your_icon.svg
```

**Solution:**
1. Check Figma frame actually has content
2. Ensure frame is not hidden in Figma
3. Re-export specific asset:
   ```bash
   npm run figma:export
   ```

---

## Processing Issues

### Color conversion not working

**Symptoms:**
```
⚠️  Multiple colors found (3), skipping currentColor conversion
```

**Diagnosis:**
Check export manifest:
```bash
cat scripts/export-manifest.json | jq '.assets[] | select(.name=="your_icon") | .metadata'
```

Look for:
```json
{
  "colors": ["#FFFFFF", "#000000", "#FF0000"],
  "colorCount": 3,
  "tintable": false
}
```

**Solution:**

**Option 1: Simplify in Figma**
1. Open icon in Figma
2. Select all layers
3. Make single color (e.g., all white)
4. Re-export

**Option 2: Manual SVG editing**
```bash
# Edit SVG file
nano third_party/melechdsp-hq/shared/mdsp_ui/ui_assets/icons/your_icon.svg

# Replace all fill/stroke colors with currentColor:
# Before: fill="#FFFFFF"
# After:  fill="currentColor"
```

**Option 3: Accept as multi-color**
- Move to illustrations directory
- Colors will be preserved
- No tinting support

---

### SVGO optimization too aggressive

**Symptoms:**
- Icon looks different after processing
- Missing details or paths

**Diagnosis:**
```bash
# Compare before and after
diff <(cat original.svg) <(cat processed.svg)
```

**Solution:**

**Option 1: Disable specific SVGO plugins**

Edit `scripts/process-svgs.js` and modify SVGO config:
```javascript
plugins: [
  {
    name: 'preset-default',
    params: {
      overrides: {
        // Disable problematic plugins
        mergePaths: false,
        convertShapeToPath: false
      }
    }
  }
]
```

**Option 2: Skip optimization for specific file**
```bash
# Restore original
cp backup/your_icon.svg third_party/melechdsp-hq/shared/mdsp_ui/ui_assets/icons/

# Process other files manually
npm run svg:process
```

---

### Validation errors

**Symptoms:**
```
❌ Errors: 2
  your_icon.svg:
    - Contains SVG animations (not supported by JUCE)
    - Contains external references (not supported)
```

**Diagnosis:**
```bash
# Run validation to see all issues
npm run svg:validate

# Check specific file
cat third_party/melechdsp-hq/shared/mdsp_ui/ui_assets/icons/your_icon.svg | grep -i "animate"
```

**Solution by error type:**

**Animation detected:**
```svg
<!-- Remove animation elements -->
<animate attributeName="opacity" ... />  ❌ Remove this
<animateTransform type="rotate" ... />   ❌ Remove this
```

**External references:**
```svg
<!-- Remove external images -->
<image xlink:href="https://..." />       ❌ Remove this
```

**Filters:**
```svg
<!-- Remove or flatten filters -->
<filter id="blur">...</filter>           ❌ Remove or flatten
```

**Manual fix:**
```bash
# Edit SVG and remove unsupported elements
nano third_party/melechdsp-hq/shared/mdsp_ui/ui_assets/icons/your_icon.svg

# Re-validate
npm run svg:validate
```

---

### File size too large

**Symptoms:**
```
⚠️  your_icon.svg: 67KB (exceeds 50KB limit)
```

**Diagnosis:**
```bash
# Check file size
ls -lh third_party/melechdsp-hq/shared/mdsp_ui/ui_assets/icons/your_icon.svg

# Check path count
grep -o '<path' your_icon.svg | wc -l
```

**Solution:**

**In Figma:**
1. Select icon
2. Object → Flatten
3. Object → Path → Simplify (adjust tolerance slider)
4. Re-export

**Command-line:**
```bash
# Aggressive SVGO optimization
npx svgo your_icon.svg --multipass

# Manual path reduction (use Inkscape or Illustrator)
```

**Last resort:**
- Use PNG instead of SVG for very complex graphics
- Change prefix to `mockup/` to export as PNG

---

## Build Issues

### IconIds.generated.h not created

**Symptoms:**
```
fatal error: mdsp_ui/IconIds.generated.h: No such file or directory
```

**Diagnosis:**
```bash
# Check if icons directory exists
ls third_party/melechdsp-hq/shared/mdsp_ui/ui_assets/icons/

# Check CMake output for generation step
grep -i "icon" build/CMakeFiles/CMakeOutput.log
```

**Solution:**
```bash
# Ensure icons directory has SVG files
ls third_party/melechdsp-hq/shared/mdsp_ui/ui_assets/icons/*.svg

# If empty, export from Figma
npm run figma:export

# Clean and rebuild
rm -rf build
./scripts/build.sh
```

---

### Icon enum not found

**Symptoms:**
```
error: 'your_icon' is not a member of 'mdsp_ui::IconId'
```

**Diagnosis:**
```bash
# Check generated header
cat third_party/melechdsp-hq/shared/mdsp_ui/include/mdsp_ui/IconIds.generated.h

# Look for your icon enum
grep "your_icon" third_party/melechdsp-hq/shared/mdsp_ui/include/mdsp_ui/IconIds.generated.h
```

**Solution:**

**Check naming:**
- File: `your_icon.svg` → Enum: `IconId::your_icon`
- Use snake_case: `your_icon` ✅ not `yourIcon` ❌

**Rebuild:**
```bash
# Regenerate IconIds
cmake --build build --target mdsp_ui_icon_ids

# Or full rebuild
./scripts/build.sh
```

---

### CMake can't find icon directory

**Symptoms:**
```
CMake Error: Icon directory not found: /path/to/icons
Run: npm run figma:export
```

**Diagnosis:**
```bash
# Check if directory exists
ls -la third_party/melechdsp-hq/shared/mdsp_ui/ui_assets/

# Check if melechdsp-hq submodule is initialized
git submodule status
```

**Solution:**
```bash
# Initialize submodule
git submodule update --init --recursive

# Create icons directory
mkdir -p third_party/melechdsp-hq/shared/mdsp_ui/ui_assets/icons

# Export icons
cd scripts
npm run figma:export
```

---

## Runtime Issues

### Icon returns nullptr

**Symptoms:**
```cpp
const auto* icon = ui_.icons().get(IconId::your_icon);
// icon == nullptr
```

**Diagnosis:**
```cpp
// Add debug logging
DBG("Attempting to load icon: " << static_cast<int>(IconId::your_icon));

const auto* icon = ui_.icons().get(IconId::your_icon);
if (icon == nullptr) {
    DBG("Icon not found in cache!");
}
```

**Check console for IconCache errors:**
```
IconCache: Icon not found: 42
```

**Solution:**

**1. Verify icon exists in binary:**
```bash
# Check BinaryData symbols
grep "your_icon" build/mdsp_ui_icons_BinaryData.h
```

**2. Verify enum value:**
```bash
# Check generated header
grep "your_icon" third_party/melechdsp-hq/shared/mdsp_ui/include/mdsp_ui/IconIds.generated.h
```

**3. Rebuild with clean:**
```bash
rm -rf build
./scripts/build.sh
```

---

### Icon displays but looks wrong

**Symptoms:**
- Icon visible but distorted
- Wrong aspect ratio
- Missing parts

**Diagnosis:**
```bash
# Check SVG viewBox
grep viewBox third_party/melechdsp-hq/shared/mdsp_ui/ui_assets/icons/your_icon.svg
```

**Solution:**

**Fix viewBox:**
```svg
<!-- Should match icon size -->
<svg viewBox="0 0 24 24" width="24" height="24">
```

**Check drawing code:**
```cpp
// Ensure bounds are correct
juce::Rectangle<float> iconBounds(x, y, 24, 24);  // Match icon size

// Use centred placement
tintedIcon->drawWithin(
    g,
    iconBounds,
    juce::RectanglePlacement::centred,  // Important!
    1.0f
);
```

---

## Tinting Issues

### Icon doesn't change color

**Symptoms:**
- `makeTinted()` returns icon but color doesn't change
- Icon always displays in original color

**Diagnosis:**
```bash
# Check if SVG uses currentColor
grep -i "currentColor" third_party/melechdsp-hq/shared/mdsp_ui/ui_assets/icons/your_icon.svg
```

**Should see:**
```svg
<path fill="currentColor" .../>
```

**If not:**
```svg
<path fill="#FFFFFF" .../>  ❌ Won't tint
```

**Solution:**

**Re-process SVG:**
```bash
npm run svg:process
```

**Manual fix:**
```bash
# Edit SVG
nano third_party/melechdsp-hq/shared/mdsp_ui/ui_assets/icons/your_icon.svg

# Replace:
fill="#FFFFFF"  →  fill="currentColor"
stroke="#FFFFFF"  →  stroke="currentColor"
```

**Rebuild:**
```bash
cmake --build build
```

---

### Tinting creates artifacts

**Symptoms:**
- Icon has visual artifacts after tinting
- Colors bleed or multiply

**Diagnosis:**
```cpp
// Check if using makeTinted correctly
auto tinted = ui_.icons().makeTinted(
    IconId::your_icon,
    juce::Colours::red,
    1.0f
);
```

**Solution:**

**Check for multiple color definitions:**
```bash
# SVG should have ONLY currentColor
grep -i "fill=" your_icon.svg
```

**Remove all hardcoded colors:**
```svg
<!-- Before -->
<g fill="#FFFFFF">
  <path fill="#FFFFFF" .../>
</g>

<!-- After -->
<g fill="currentColor">
  <path fill="currentColor" .../>
</g>
```

---

### Gradients not tinting

**Symptoms:**
- Icon with gradient doesn't respond to tinting

**Diagnosis:**
```bash
# Check for gradients
grep -i "gradient" your_icon.svg
```

**Solution:**
**Gradients cannot be tinted dynamically.** Options:

**Option 1: Convert to solid color in Figma**
1. Flatten gradient to solid color
2. Re-export

**Option 2: Accept as-is**
- Move to illustrations directory
- Use without tinting

**Option 3: Use multiple icon variants**
- Create separate icons for each color
- Export as `icon/your_icon_red`, `icon/your_icon_blue`, etc.

---

## Performance Issues

### Slow icon loading at startup

**Symptoms:**
- Plugin startup delay
- Long initialization time

**Diagnosis:**
```cpp
// Add timing measurement
auto start = juce::Time::getMillisecondCounterHiRes();
const auto* icon = ui_.icons().get(IconId::your_icon);
auto elapsed = juce::Time::getMillisecondCounterHiRes() - start;
DBG("Icon load time: " << elapsed << "ms");
```

**Solution:**

**Lazy loading is already implemented** in IconCache. If slow:

1. **Reduce SVG complexity:**
   ```bash
   # Check file sizes
   ls -lh third_party/melechdsp-hq/shared/mdsp_ui/ui_assets/icons/

   # Simplify large files in Figma
   ```

2. **Profile icon loading:**
   ```cpp
   // Preload frequently-used icons
   void preloadIcons() {
       ui_.icons().get(IconId::chevron_down);
       ui_.icons().get(IconId::play);
       // ... other common icons
   }
   ```

---

### High memory usage

**Symptoms:**
- Plugin uses excessive RAM
- Memory grows over time

**Diagnosis:**
```cpp
// Check cache size
DBG("Icon cache size: " << ui_.icons().getCacheSize());
```

**Solution:**

IconCache caches parsed drawables. If memory is an issue:

1. **Reduce icon count:**
   - Remove unused icons from exports

2. **Check for leaks:**
   ```cpp
   // Ensure proper cleanup
   auto tinted = ui_.icons().makeTinted(...);
   // Don't keep references longer than needed
   ```

3. **Use get() instead of makeTinted() when possible:**
   ```cpp
   // Zero-allocation path (if not tinting):
   const auto* icon = ui_.icons().get(IconId::your_icon);
   ```

---

## Advanced Debugging

### Enable verbose logging

**Export:**
```bash
npm run figma:export -- --verbose
```

**Processing:**
```bash
npm run svg:process -- --verbose
```

**Runtime:**
```cpp
// In IconCache.cpp, add:
DBG("Loading icon: " << iconIdToResourceName(id));
```

---

### Inspect export manifest

```bash
# View full manifest
cat scripts/export-manifest.json | jq .

# Check specific icon
cat scripts/export-manifest.json | jq '.assets[] | select(.name=="your_icon")'

# List all non-tintable icons
cat scripts/export-manifest.json | jq '.assets[] | select(.metadata.tintable==false)'
```

---

### Manual SVG testing

Test SVG in browser before embedding:

```html
<!DOCTYPE html>
<html>
<body style="background: #333;">
  <!-- Test currentColor -->
  <svg width="48" height="48" style="color: red;">
    <!-- Paste your SVG content here -->
  </svg>

  <svg width="48" height="48" style="color: blue;">
    <!-- Same SVG, different color -->
  </svg>
</body>
</html>
```

If `currentColor` works in browser, it will work in JUCE.

---

### CMake regeneration

Force CMake to regenerate icon headers:

```bash
# Remove generated files
rm third_party/melechdsp-hq/shared/mdsp_ui/include/mdsp_ui/IconIds.generated.h
rm build/mdsp_ui_icons_BinaryData.*

# Reconfigure
cmake --build build --target mdsp_ui_icon_ids

# Full rebuild
cmake --build build
```

---

### Git workflow debugging

```bash
# Check which files changed
git status third_party/melechdsp-hq/shared/mdsp_ui/ui_assets/icons/

# View SVG diff
git diff third_party/melechdsp-hq/shared/mdsp_ui/ui_assets/icons/your_icon.svg

# Restore from git if needed
git checkout third_party/melechdsp-hq/shared/mdsp_ui/ui_assets/icons/your_icon.svg
```

---

## Common Error Messages

### "FIGMA_TOKEN not set"
```
❌ Figma token not found. Set FIGMA_TOKEN in .env file
```
**Fix:** Create `.env` file with `FIGMA_TOKEN=your_token`

---

### "No SVG files found"
```
⚠️  No SVG files found to process
```
**Fix:** Run `npm run figma:export` first

---

### "Validation failed"
```
❌ Processing failed: Validation errors found
```
**Fix:** Run `npm run svg:validate` to see specific issues

---

### "Icon directory not found"
```
CMake Error: Icon directory not found
```
**Fix:** Initialize submodule: `git submodule update --init`

---

## Getting More Help

### Check documentation

- [FIGMA_SVG_WORKFLOW.md](./FIGMA_SVG_WORKFLOW.md) - Main workflow guide
- [FIGMA_SETUP.md](./FIGMA_SETUP.md) - Figma file setup
- [ASSET_TYPE_EXAMPLES.md](./ASSET_TYPE_EXAMPLES.md) - Visual examples

### Diagnostic scripts

```bash
# Full diagnostic
./scripts/diagnose-icons.sh  # (create this for automated checks)

# Or manual checks:
echo "=== Icon Directory ==="
ls -lh third_party/melechdsp-hq/shared/mdsp_ui/ui_assets/icons/

echo "=== Export Manifest ==="
cat scripts/export-manifest.json | jq '.totalAssets, .errors, .warnings | length'

echo "=== Generated Header ==="
grep "enum class IconId" third_party/melechdsp-hq/shared/mdsp_ui/include/mdsp_ui/IconIds.generated.h

echo "=== Binary Data ==="
ls -lh build/mdsp_ui_icons_BinaryData.*
```

---

## Summary Checklist

When troubleshooting, check in this order:

- [ ] **Folder paths** match expected layout: `third_party/melechdsp-hq/shared/mdsp_ui/ui_assets/icons/` (see [Folder structure and naming](#folder-structure-and-naming))
- [ ] Figma frames named correctly (`icon/` prefix)
- [ ] Personal access token valid and in `.env`
- [ ] File ID correct in `figma-export.config.json`
- [ ] `outputDir` in config is relative to `scripts/` and points at your real folders
- [ ] Icons exported successfully (`ls third_party/melechdsp-hq/shared/mdsp_ui/ui_assets/icons/`)
- [ ] SVGs processed successfully (`npm run svg:validate`)
- [ ] Build completed without errors
- [ ] `IconIds.generated.h` contains your icon enum
- [ ] Icon SVG uses `currentColor` for tinting
- [ ] Runtime code using correct `IconId` enum value

**Still stuck?** Check the export manifest for detailed diagnostics:
```bash
cat scripts/export-manifest.json | jq '.assets[] | select(.name=="your_icon")'
```
