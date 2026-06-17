# Cursor task — fix Metal peak-hold (flat-line bug) + make the hold ceiling distinct

The previous change pointed Metal's peak-hold at `renderSnapshot.fftPeakHoldDb`, but the engine's
peak-hold is DISABLED (`Source/PluginProcessor.cpp:239` → `PeakHoldMode::Off`), so that field is
never populated → Metal drew a flat line at the top of the scale. The real, working peak-hold the
CPU shows is the shared builder's `uiHeldPeak` latch
(`third_party/melechdsp-hq/shared/mdsp_gui/src/analyzer/AnalyzerRenderStateBuilder.cpp` ~lines 80-89),
latched from the PEAK trace with the Hold flag:
```
hold ON  → held = max(held, peak)   // instant attack, frozen session-max
hold OFF → held = peak              // tracks the live peak
```
Mirror that on the Metal render thread (we can't read the message-thread provider, so we replicate
its algorithm against the lock-free snapshot's peak). Do NOT re-introduce any decay term.

## Change 1 — `Source/ui/analyzer/metal/MetalHost.mm`, `updateAnalyzerPipelineFromSnapshot`

**Reset branch (~line 1322):** the line currently reads
```objcpp
analyzerPeakHoldDb[i] = sanitizeAnalyzerDb (renderSnapshot.fftPeakHoldDb[i]);
```
Change to prime the hold to the live peak (already finalized just above in this branch):
```objcpp
analyzerPeakHoldDb[i] = analyzerPeakDb[i];
```

**Main per-bin block (~line 1339):** currently
```objcpp
analyzerPeakHoldDb[i] = sanitizeAnalyzerDb (renderSnapshot.fftPeakHoldDb[i]);

const float rmsFloorDb = analyzerSmoothedDb[i];
analyzerPeakDb[i] = sanitizeAnalyzerDb (juce::jmax (analyzerPeakDb[i], rmsFloorDb));
```
Replace with (clamp peak to the rms floor FIRST, then latch the hold off the clamped peak):
```objcpp
const float rmsFloorDb = analyzerSmoothedDb[i];
analyzerPeakDb[i] = sanitizeAnalyzerDb (juce::jmax (analyzerPeakDb[i], rmsFloorDb));

// Peak-hold = the latched peak envelope (mirrors AnalyzerRenderStateBuilder's uiHeldPeak):
// instant attack; frozen session-max while Hold is engaged; tracks the live peak when off
// (only drawn while Hold is on, so the off case just keeps it primed).
if (renderSnapshot.isHoldOn)
    analyzerPeakHoldDb[i] = sanitizeAnalyzerDb (juce::jmax (analyzerPeakHoldDb[i], analyzerPeakDb[i]));
else
    analyzerPeakHoldDb[i] = analyzerPeakDb[i];
```
`renderSnapshot.fftPeakHoldDb` is now unused in this function — that's fine (leave the engine field
alone; do NOT enable the engine peak-hold mode, it would change the CPU path).

## Change 2 — make the hold ceiling visually distinct (`Source/ui/analyzer/AnalyzerDisplayView.cpp`)

Currently `theme_.seriesHold` is forced equal to `theme_.seriesPeak` (search for
`theme_.seriesHold = theme_.seriesPeak` — appears ~line 174 and ~line 193). Because the live Peak and
the hold ceiling share the exact same colour, the frozen ceiling sits invisibly on top of the Peak
line (this is what read as "peak/hold disappeared" earlier). Mirror the CPU treatment and brighten the
hold so it reads as a separate ceiling:
```cpp
theme_.seriesHold = theme_.seriesPeak.brighter (0.3f);
```
Apply at BOTH assignment sites (preserve each site's existing alpha handling — only change the hue/
brightness, keep `.withAlpha(...)` if present so the relative opacity is unchanged).

## Acceptance (Claude builds/signs/installs; human verifies in PT)
- Peak on, Hold on → a frozen, brighter ceiling sits ABOVE the still-moving Peak, snaps up to new
  peaks instantly (no lag, no flat line, not pinned to 0 dB). Peak stays visible and distinct.
- Hold off → no ceiling; Peak unaffected.
- Toggling Hold on/off repeatedly is clean; no crash (the deferred last-trace guard is unrelated).
- Matches the CPU/Windows look.
