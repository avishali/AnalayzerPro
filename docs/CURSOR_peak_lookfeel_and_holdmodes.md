# Cursor task — Peak/Hold redesign: always-on filled Peak, 2-mode hold, restore pre-Metal look

Four user requirements (decisions already made):
1. **Peak always visible & on top** — revert the channel auto-hide; Peak shows per Show Peak toggle,
   has its OWN translucent fill, drawn ON TOP of RMS and channel traces (never buried).
2. **Peak-hold always visible when Show Peak is on**, in BOTH modes.
3. **Peak-hold has 2 modes** — Hold button ON = infinite max, stays until Reset; Hold button OFF =
   decay: held peaks fall back toward the live peak over the Release time.
4. **Look & feel** — thicker strokes (esp. Peak), glow + soft shadow + bright highlight, and a real
   fill — restore the richer pre-Metal CPU look (`drawSilkTrace` in
   `third_party/melechdsp-hq/shared/mdsp_ui/src/rta/RtaPaintUtils.cpp`). Metal currently renders a
   thin ~1.8px core + one faint glow, no highlight → reads thin/flat.

Keep ARC-off rules in `.mm`. Values below are STARTING POINTS — the user will fine-tune the look.

---

## PART A — revert auto-hide; Peak always-on, filled, on top (`AnalyzerDisplayView.{h,cpp}`)

1. **Remove the auto-hide gating** added last task:
   - Peak trace (~line 620): visible arg back to just `showPeak_` (drop `&& ! anyChannelTraceEnabled()`).
   - Peak-hold trace (~line 634): visible arg to just `showPeak_` (drop BOTH `isHoldOn_` AND
     `&& ! anyChannelTraceEnabled()`). Per decision #2 the ceiling shows in both modes whenever Show
     Peak is on — visibility no longer depends on the Hold button.
   - CPU `setFFTData` feed (~lines 1468-1469): drop the `&& ! anyChannelTraceEnabled()` terms (keep
     `usePeaks && showPeak_` for peaks, `showPeak_ && …usePeakHold()` for peak-hold).
   - Delete the now-unused `anyChannelTraceEnabled()` helper from the header.

2. **Peak gets its own fill, always** (~line 617-625 `setTrace (frame.peakTrace, …)`): change the
   `fillToBottom` arg from `! anyNonPeakTraceEnabled` to `true`, and use a clearly translucent
   gradient so RMS/channels remain visible underneath, e.g. top alpha `0.22`, bottom `0.04`.

3. **Draw order = Peak on top.** In `MetalHost.mm` `drawAnalyzerFrame` the order is already
   side/mid/left/right/mono/stereo → RMS → Peak → Peak-hold. Keep that (Peak after RMS/channels =
   its fill composites on top). Just confirm Peak-hold draws LAST.

---

## PART B — Peak-hold 2 modes (`MetalHost.mm`, `updateAnalyzerPipelineFromSnapshot`)

Replace the current single-mode latch. `frame.rmsReleaseMs` carries the "Release" (PeakDecay) knob.

- **Reset branch (~line 1322):** `analyzerPeakHoldDb[i] = analyzerPeakDb[i];` (unchanged).
- **Decay rate (compute once per frame, near the top of the function where `renderDtSeconds` is in
  scope):**
  ```objcpp
  const float holdReleaseMs       = juce::jmax (1.0f, frame.rmsReleaseMs);
  const float holdDecayDbPerSec   = 60000.0f / holdReleaseMs;            // ~60 dB over the release time
  const float holdDecayThisFrame  = holdDecayDbPerSec * static_cast<float> (juce::jmax (0.0, renderDtSeconds));
  ```
- **Main per-bin block** (after the existing `analyzerPeakDb[i] = max(peakDb, rmsFloor)` clamp):
  ```objcpp
  if (renderSnapshot.isHoldOn)
  {
      // Mode 1 — infinite hold: latch the running max, never decay (Reset clears it).
      analyzerPeakHoldDb[i] = sanitizeAnalyzerDb (juce::jmax (analyzerPeakHoldDb[i], analyzerPeakDb[i]));
  }
  else
  {
      // Mode 2 — decay: held peak falls toward the live peak over the Release time.
      const float decayed = analyzerPeakHoldDb[i] - holdDecayThisFrame;
      analyzerPeakHoldDb[i] = sanitizeAnalyzerDb (juce::jmax (analyzerPeakDb[i], decayed));
  }
  ```
  (Instant attack is implicit: `analyzerPeakDb` is the live peak and the `jmax` snaps the hold up to
  it immediately in both modes.) "Reset" already routes through `resetViewPeaks()`.

---

## PART C — restore the pre-Metal look (`MetalHostShared.h`, `MetalHost.mm`, `AnalyzerDisplayView.cpp`)

### C1. Per-trace stroke controls — `MetalHostShared.h` `struct MetalTracePayload`
Add:
```cpp
float strokeWidthPx = 1.8f;   // base core thickness (pre-widthScale)
bool  emphasize     = false;  // Peak: wider glow + bright highlight pass
```

### C2. Set them per trace — `AnalyzerDisplayView.cpp` (the `setTrace` calls)
Either extend the `setTrace` lambda with two trailing params or assign the fields right after each
call. Starting values:
- Peak: `strokeWidthPx = 2.8f; emphasize = true;`
- Peak-hold: `strokeWidthPx = 2.2f; emphasize = false;` (already brighter colour)
- RMS: `strokeWidthPx = 2.4f;`
- Channels (L/R/Mid/Side/Mono/Stereo): `strokeWidthPx = 2.0f;`

### C3. Richer stroke — `MetalHost.mm` `drawTracePayloadFromDb` (~lines 1745-1760)
Replace the fixed-width single-glow block. Use the trace width + add a wide soft shadow under the
glow and a bright highlight on top for emphasized traces (mirrors `drawSilkTrace`):
```objcpp
if (trace.strokeVisible && lineVertexCount >= 2)
{
    const float plotW      = frame.plotRectPx.w;
    const float widthScale = juce::jlimit (0.9f, 1.4f, 0.9f + 0.0015f * plotW);
    const float base       = (trace.strokeWidthPx > 0.0f ? trace.strokeWidthPx : 1.8f);
    const float coreHalf   = 0.5f * base * widthScale;
    const float glowHalf   = coreHalf * (trace.emphasize ? 3.2f : 2.8f);
    const float shadowHalf = coreHalf * 5.5f;                  // wide soft bloom/shadow
    const float coreAlpha  = trace.colour.a * (trace.emphasize ? 0.95f : 0.82f);
    const float glowAlpha  = trace.colour.a * (trace.emphasize ? 0.13f : 0.10f);
    const float shadowAlpha= trace.colour.a * 0.05f;

    // back-to-front: soft shadow → glow halo → solid core → bright highlight
    emitGlowRibbon (encoder, lineVertexCount, shadowHalf, trace.colour, shadowAlpha, drawableWidth, drawableHeight);
    emitGlowRibbon (encoder, lineVertexCount, glowHalf,   trace.colour, glowAlpha,   drawableWidth, drawableHeight);
    emitGlowRibbon (encoder, lineVertexCount, coreHalf,   trace.colour, coreAlpha,   drawableWidth, drawableHeight);

    if (trace.emphasize)
    {
        MetalColour hi = trace.colour;            // brighten ~0.18 toward white, keep RGB premultiply consistent
        hi.r = juce::jmin (1.0f, hi.r + 0.18f);
        hi.g = juce::jmin (1.0f, hi.g + 0.18f);
        hi.b = juce::jmin (1.0f, hi.b + 0.18f);
        const float hiHalf  = coreHalf * 0.5f;
        const float hiAlpha = trace.colour.a * 0.30f;
        emitGlowRibbon (encoder, lineVertexCount, hiHalf, hi, hiAlpha, drawableWidth, drawableHeight);
    }
    didDraw = true;
}
```
Leave the older `drawTracePayload` (non-DB fallback) as-is — the pipeline/DB path is what renders in
PT. Do not change `emitGlowRibbon` itself.

---

## Acceptance (Claude builds/signs/installs; human verifies in PT)
- Peak is clearly THICKER with visible glow/shadow/highlight + a translucent fill, sitting ON TOP of
  RMS and any channel traces (not buried). RMS/channels also a touch thicker.
- Enable channels → Peak still shows on top. (Auto-hide gone.)
- Hold ON → peak-hold ceiling freezes at the session max and stays until Reset. Hold OFF → the
  ceiling slowly falls back toward the live peak over the Release time. Ceiling visible in both when
  Show Peak is on, in its brighter colour.
- No crash; never-blank guard unchanged.
