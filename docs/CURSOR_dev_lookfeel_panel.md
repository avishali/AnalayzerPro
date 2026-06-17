# Cursor task — DEV look-and-feel tuning panel (live, like MasterLimiter's DevWindow)

Add a developer-only window of sliders that tweak the Metal look-and-feel constants (glow, shadow,
highlight, stroke widths, fills, meter/scope glow) **live in PT** — no rebuild per tweak — plus a
"Copy values" button to bake the chosen numbers. Mirror MasterLimiter's pattern
(`MelechDSP/MasterLimiter/Source/PluginEditor.{h,cpp}` `DevWindow` + `Source/ui/DevControlsComponent.{h,cpp}`,
toggled via `MainView::onToggleDevControls`).

Unlike MasterLimiter (APVTS dev params → DSP), our look constants are consumed on the Metal RENDER
THREAD, so route them through a plain **tunables struct copied into the frame each tick** (same path
as `peakHoldDecayMs`). These are dev-only: NOT APVTS, NOT saved, NOT automatable.

Gate the whole panel behind a macro so it compiles out for release:
```cpp
#ifndef ANALYZERPRO_DEV_LOOK_PANEL
 #define ANALYZERPRO_DEV_LOOK_PANEL 1   // set to 0 (or remove) before shipping
#endif
```

## 1. Tunables struct — `Source/ui/analyzer/metal/MetalHostShared.h`
Add (defaults = the CURRENT hardcoded values so behaviour is unchanged until a slider moves):
```cpp
struct MetalLookTunables
{
    // analyzer trace stroke (drawTracePayloadFromDb)
    float glowMultEmph   = 3.2f,  glowMultNorm = 2.8f;
    float glowAlphaEmph  = 0.13f, glowAlphaNorm = 0.10f;
    float shadowMult     = 5.5f,  shadowAlpha   = 0.05f;
    float coreAlphaEmph  = 0.95f, coreAlphaNorm = 0.82f;
    float hiMult         = 0.5f,  hiAlpha       = 0.30f, hiBrighten = 0.18f;
    // peak / hold fills (set in AnalyzerDisplayView, but expose here for tuning)
    float peakFillTop = 0.22f, peakFillBot = 0.04f;
    float holdFillTop = 0.18f, holdFillBot = 0.03f;
    // meters (drawMeterBars)
    float meterGlowMargin = 8.0f, meterHaloTop = 0.38f, meterHaloBot = 0.06f, meterCapGlowAlpha = 0.15f;
    // phase-fan (drawPhaseFanFrame)
    float phaseFanGlowScale = 1.11f, phaseFanGlowAlpha = 0.22f;
    // goniometer (drawGonioFrame)
    float gonioGlowMult = 4.0f, gonioGlowAlpha = 0.22f;
};
```
Add a field to `MetalAnalyzerFrame`: `MetalLookTunables look;`

## 2. Source of truth + frame copy — `AnalyzerDisplayView.{h,cpp}`
- Member: `AnalyzerPro::metal::MetalLookTunables lookTunables_;`
- Accessor: `AnalyzerPro::metal::MetalLookTunables& devLookTunables() noexcept { return lookTunables_; }`
- In `fillMetalAnalyzerFrame`: `frame.look = lookTunables_;`
- Where the peak/hold fills are currently literals, drive them from `lookTunables_` (peakFillTop/Bot,
  holdFillTop/Bot) so those sliders work too.

## 3. Consume in render — `MetalHost.mm`
Replace the hardcoded constants with `frame.look.*` (thread the frame's `look` into the stroke/meter/
scope draws):
- `drawTracePayloadFromDb` stroke block: `glowMult`/`glowAlpha`/`coreAlpha` pick Emph vs Norm by
  `trace.emphasize`; `shadowMult`, `shadowAlpha`, `hiMult`, `hiAlpha`, `hiBrighten` from `frame.look`.
- `drawMeterBars`: `kMeterGlowMargin` → `frame.look.meterGlowMargin`; halo alphas →
  `meterHaloTop`/`meterHaloBot`; cap glow alpha → `meterCapGlowAlpha`.
- `drawPhaseFanFrame`: `kGlowScale` → `phaseFanGlowScale`, glow vertex alpha → `phaseFanGlowAlpha`.
- `drawGonioFrame` glow quad: `baseHalf * 4.0f` → `* frame.look.gonioGlowMult`; `kGlowAlpha` →
  `gonioGlowAlpha`.
(These draw fns already receive the frame — read `frame->look`.)

## 4. Dev panel component — `Source/ui/dev/DevLookControlsComponent.{h,cpp}` (NEW, gated)
A `juce::Component` holding labelled `juce::Slider`s (rotary or linear) grouped by section
(Traces / Fills / Meters / Phase-fan / Gonio), constructed with a
`AnalyzerPro::metal::MetalLookTunables&` reference. On each slider change, write the value into the
referenced struct and call a supplied `std::function<void()> onChanged` (to trigger a repaint). Put it
in a `juce::Viewport` so it scrolls. Sensible ranges/steps per field (e.g. alphas 0–1 step .01, mults
1–8 step .05, margins 0–24 step .5). Add a **"Copy values"** `TextButton` that formats the current
struct as a C++ aggregate initializer and puts it on the clipboard
(`juce::SystemClipboard::copyTextToClipboard`) so the dialed-in look can be baked into the defaults.

## 5. Window + toggle — `PluginEditor.{h,cpp}` + `MainView`
- Add `std::function<void()> onToggleDevLookPanel;` to `MainView`; fire it from `MainView::keyPressed`
  on a combo, e.g. **Cmd+Shift+L** (only when `ANALYZERPRO_DEV_LOOK_PANEL`).
- In `PluginEditor`: a `DevLookWindow : juce::DocumentWindow` (mirror MasterLimiter's `DevWindow`),
  `toggleDevLookPanel()` creates/destroys it, `setContentOwned(new DevLookControlsComponent(
  mainView_.analyzerDevLookTunables(), [this]{ mainView_.repaint(); }), true)`. Expose
  `MetalLookTunables& MainView::analyzerDevLookTunables()` → `analyzerView_.devLookTunables()`.
- Wire `mainView_.onToggleDevLookPanel = [this]{ toggleDevLookPanel(); };`

## Acceptance
- Cmd+Shift+L opens a scrollable DEV window; moving any slider changes the look in PT **immediately**
  (no rebuild). "Copy values" puts a pasteable initializer on the clipboard. Defaults match today's
  look. `ANALYZERPRO_DEV_LOOK_PANEL 0` compiles the whole thing out. No render-thread allocations added
  (the struct is a trivially-copyable POD copied into the frame).
