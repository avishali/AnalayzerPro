# FIGMA TOKEN IMPORT — Status & Requirements

## STEP 0: Session Initialization — ✅ COMPLETE

### Current State Analysis

**Theme Structure (`mdsp_ui::Theme`)**:
- ✅ Dark theme implemented (default values in Theme.cpp)
- ✅ Light theme placeholder exists (inverted dark theme, needs Figma tokens)
- ✅ ThemeVariant enum exists (Dark/Light)
- ✅ UiContext supports variant switching

**Current Theme Fields** (ready for token mapping):
```cpp
// Backgrounds
background, panel

// Grid/guides
grid, gridMajor

// Text
text, textMuted

// Accents
accent, seriesPeak, warning, error, danger, success

// UI component colors
lightGrey, grey, darkGrey, black, transparentBlack, borderDivider
```

**Metrics Structure (`mdsp_ui::Metrics`)**:
- ✅ All spacing/padding metrics defined
- ✅ Component sizes defined
- ✅ Radii/strokes defined
- ✅ Ready for token value updates

**Typography Structure (`mdsp_ui::Typography`)**:
- ✅ Font sizes defined
- ✅ Helper methods for font creation
- ✅ Ready for token value updates

---

## STEP 1: COLOR TOKEN MAPPING — ⏳ PENDING

### Requirements

**Needed**: Figma color token data in one of these formats:
- JSON export (e.g., `design-tokens.json`)
- CSV/TSV file
- Plain text list with color values
- Figma API response

### Expected Token Format

The runbook expects tokens mapped to these semantic categories:

**Dark Theme Colors**:
```json
{
  "colors": {
    "dark": {
      "background": "#121212",
      "panel": "#1a1a1a",
      "grid": "#3a3a3a",
      "gridMajor": "#4a4a4a",
      "text": "#e6e6e6",
      "textMuted": "#b0b0b0",
      "accent": "#4fc3f7",
      "warning": "#ffc107",
      "error": "#ff5252",
      ...
    },
    "light": {
      "background": "...",
      "panel": "...",
      ...
    }
  }
}
```

**Action Required**:
1. Provide Figma color token export (JSON preferred)
2. Or specify which Figma file/component contains the tokens
3. Or provide a color palette document/mapping

---

## STEP 2: METRICS TOKEN MAPPING — ⏳ PENDING

### Requirements

**Needed**: Figma spacing/radius token values:
- Spacing scale (4px, 8px, 12px, 16px, etc.)
- Component heights (button, combo, slider)
- Border radius values (small, medium, large)
- Stroke widths (thin, medium, thick)

### Current Metrics (to be updated from tokens):

```cpp
// Padding / spacing
pad = 10           // Standard padding
padSmall = 4       // Small padding
gap = 8            // Standard gap
gapSmall = 2       // Small gap
gapTiny = 3        // Tiny gap
sectionSpacing = 8 // Spacing between sections

// Component sizes
titleHeight = 14
secondaryHeight = 12
buttonH = 22
buttonW = 75
comboH = 16
sliderH = 14
...

// Radii / strokes
rSmall = 2.0f
rMed = 4.0f
rLarge = 8.0f
strokeThin = 1.0f
strokeMed = 1.5f
strokeThick = 2.0f
```

**Action Required**:
1. Provide Figma spacing/radius token values
2. Or specify which Figma design system contains spacing tokens
3. Or provide spacing scale document

---

## STEP 3: TYPOGRAPHY TOKEN MAPPING — ⏳ PENDING

### Requirements

**Needed**: Figma typography scale values:
- Font sizes for each text style (title, section, label, etc.)
- Font families (optional - can be stubbed initially)
- Line heights (optional - can use default)

### Current Typography (to be updated from tokens):

```cpp
titleH = 14.0f       // Main title
sectionTitleH = 12.0f // Section titles
labelH = 11.0f       // Standard labels
labelSmallH = 9.0f   // Small labels
placeholderH = 12.0f // Placeholder text
statusH = 11.0f      // Status text
```

**Action Required**:
1. Provide Figma typography scale values
2. Or specify which Figma text styles to use
3. Or provide typography scale document

---

## STEP 4: VALIDATION — ⏳ PENDING

### Planned Validation Steps

Once tokens are imported:
1. ✅ Verify AnalyzerPro compiles with new token values
2. ✅ Test Dark theme (should match existing appearance)
3. ✅ Test Light theme (new token values)
4. ✅ Verify no UI layout breakage
5. ✅ Check for visual regressions

---

## Token Import Options

### Option A: Provide Token File
If you have a Figma token export:
- JSON format preferred
- Place in `third_party/melechdsp-hq/shared/mdsp_ui/tokens/` (to be created)
- Or attach in response

### Option B: Manual Token Entry
If you have token values in another format:
- Provide as structured text/csv
- I'll map them to Theme/Metrics/Typography

### Option C: Figma API Integration
If you have Figma API access:
- Provide Figma file ID and API token (handled securely)
- I'll extract tokens via Figma API

### Option D: Extract from Design File
If tokens are in a Figma design file:
- Provide file URL or export
- I'll extract token values

---

## Next Steps

**Please provide**:
1. Figma token data (in any of the formats above), OR
2. Instructions on where to find the tokens, OR
3. Confirmation to proceed with placeholder/default tokens

Once token data is provided, I'll proceed with:
- STEP 1: Map colors to `mdsp_ui::Theme`
- STEP 2: Map spacing/radius to `mdsp_ui::Metrics`
- STEP 3: Map typography to `mdsp_ui::Typography`
- STEP 4: Validate AnalyzerPro works correctly

---

**Status**: ⏳ Waiting for Figma token data to proceed with Steps 1-3.
