# UI Overhaul 03 Result

Mission: `UI_OVERHAUL_03_LOOKFEEL_GENERALIZE`

## Source Of Truth
- Canonical edits were made in `/Users/avishaylidani/DEV/GitHubRepo/MelechDSP/melechdsp-hq`.
- Both products were configured with `-DMELECHDSP_HQ_ROOT=/Users/avishaylidani/DEV/GitHubRepo/MelechDSP/melechdsp-hq`.
- `AnalyzerPro` and `MasterLimiter` both use `third_party/melechdsp-hq` as git submodules. The release propagation step is therefore a submodule SHA bump after the canonical HQ change is committed, not a vendored file copy.

## What Moved To Shared `mdsp_ui`
- Added shared palette tokens for control fills, raised/low control states, stronger borders, accent bright/dim, and muted icon color.
- Added geometry tokens for soft buttons, rotary stroke widths, and vertical fader dimensions.
- Moved generic soft button, toggle, combo box, rotary knob, and vertical fader rendering into `mdsp_ui::LookAndFeel`, `ButtonStyle`, and `ButtonPaint`.
- Preserved the existing horizontal linear slider path.
- Kept colors and geometry token-driven through `Theme`, `Metrics`, `tokens.json`, and regenerated `ThemeTokens.generated.cpp`.

## Product-Specific Remaining Work
- `MasterLimiterLookAndFeel` now keeps only name-dispatched product-specific button cases: limiter power glyph, link icons, segmented controls, meter zoom glyphs, and a few text-state highlights.
- Generic MasterLimiter rotary, linear fader, combo, and normal button drawing now delegates to shared `mdsp_ui::LookAndFeel`.
- AnalyzerPro now lets `HeaderBar` inherit the editor-level shared `mdsp_ui::LookAndFeel` instead of installing an ad-hoc header-only LNF.

## Build Status
- `MasterLimiter` canonical-HQ build: green.
- `AnalyzerPro` canonical-HQ build: green.
- AnalyzerPro canonical config requires `-DANALYZERPRO_COPY_AFTER_BUILD=OFF` to avoid the AAX install permission step under `/Library/Application Support/Avid/...`.

## Review Notes
- This is render-only. No DSP, APVTS, parameter IDs, or audio-thread code were changed.
- Visual review is still owner-gated. Header buttons now use the shared pill style, so module/header visual balance should be checked in the plugin.
- Submodule pointer bump is not completed in this working tree because the canonical HQ changes need to be committed first, then parent products can update their `third_party/melechdsp-hq` SHAs.

STOP
