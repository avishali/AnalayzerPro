# Cursor task — Peak-hold follows peak + make Peak trace toggleable

Two independent changes in AnalyzerPro. Keep the diff tight; do not touch the present path,
teardown, descriptor handling, BackingLayer hosting, or the RMS pipeline. ARC is OFF in `.mm`
(never `release` autoreleased objects). One codebase — Mac (Metal) and Windows (CPU) must stay
behaviorally identical.

---

## Change 1 — Peak-hold must follow the PEAK trace (not RMS)

**Bug:** the Metal render-thread pipeline computes peak-hold from the RMS target, so the live Peak
trace overshoots the hold and the hold "catches up over time." The CPU path
(`third_party/melechdsp-hq/shared/mdsp_gui/src/analyzer/AnalyzerRenderStateBuilder.cpp` ~line 80)
already does the right thing: `heldDb = max(heldDb, fftPeakDb[i])` — running max of the peak.

**Fix —** `Source/ui/analyzer/metal/MetalHost.mm`, in `updateAnalyzerPipelineFromSnapshot`
(~line 1343). The peak-hold currently reads `targetDb`; change it to the live peak
`analyzerPeakDb[i]` so the hold is the running max of the Peak trace:

```objcpp
// BEFORE
analyzerPeakHoldDb[i] = sanitizeAnalyzerDb (juce::jmax (targetDb, decayedPeakHoldDb));
// AFTER
analyzerPeakHoldDb[i] = sanitizeAnalyzerDb (juce::jmax (analyzerPeakDb[i], decayedPeakHoldDb));
```

`analyzerPeakDb[i]` is already finalized one line above (it's `max(peakSourceDb, targetDb)`).
Leave the `isHoldOn` decay branch and the rmsFloor clamps below it unchanged. Do NOT re-add any
per-bin "hold == peak" clamp (that was reverted in `3ac963b` for gluing the lines). With this change
the hold sits at the running-max ceiling and the live peak bounces below it, touching only at new
maxima — not glued.

---

## Change 2 — Make the Peak trace toggleable (new `analyzerShowPeak` param, default ON)

Today the main Peak trace is hardcoded always-visible. Add a user toggle. **Default ON** so the
existing Peak-primary workflow (user runs with RMS off, views Peak) is unchanged; the user turns it
OFF to declutter when viewing only channel traces.

### 2a. Parameter — `Source/PluginProcessor.cpp`
After the "Show RMS" param block (~line 999), add:
```cpp
// Show Peak
params.push_back (std::make_unique<juce::AudioParameterBool> (
    "analyzerShowPeak", "Show Peak",
    true,   // Default: on (preserves current always-on behavior)
    "Show Peak"));
```
Mirror the cached-pointer pattern if you add one (`pTraceShowPeak_`) and add it to the existing
`juce::ignoreUnused (...)` line (~371). Optional — only if you add the member.

### 2b. ControlId — `Source/control/ControlIds.h`
Add `TraceShowPeak` next to `TraceShowRMS`.

### 2c. Param map — `Source/control/AnalyzerProParamIdMap.cpp`
Add: `m[ap::control::ControlId::TraceShowPeak] = "analyzerShowPeak";`

### 2d. UI toggle — `Source/ui/layout/ControlRail.cpp`
Add a "Show Peak" toggle row mirroring the existing "Show RMS" row (`showRmsRow` /
`showRmsButton` — see the row construction ~line 29, `attachToParent`/tooltip ~122-129, the
`bindToggle` ~236-253, and `placeTraceRow(showRmsRow, TraceId::Rms)` ~591). Bind it to
`AnalyzerPro::ControlId::TraceShowPeak`. Place it adjacent to the RMS row in the trace section.
Also add the matching row to `Source/ui/layout/SettingsPopupPanel.cpp` (it mirrors the trace rows —
see the `lRow_`/`lBtn_`/`ControlId::TraceShowL` table ~line 192) so the Settings popup stays in sync.

### 2e. View gating — `Source/ui/analyzer/AnalyzerDisplayView.{h,cpp}`
- Add member `bool showPeak_ = true;` and setter `void setShowPeak (bool) noexcept;` (mirror how
  `isHoldOn_` is stored).
- In the Metal frame builder (`AnalyzerDisplayView.cpp`):
  - Peak trace (~line 620): change the hardcoded `true` visible arg to `showPeak_`.
  - Peak-hold trace (~line 634): change visible from `isHoldOn_` to `showPeak_ && isHoldOn_`.
- In the CPU feed (`setFFTData`, ~line 1452): only feed peak / peak-hold when `showPeak_` is true, so
  the CPU/Windows path also respects the toggle:
  ```cpp
  usePeaks && showPeak_ ? &fftPeakDbDisplay_ : nullptr,
  (showPeak_ && ! peakHoldDb.empty() && renderStateProvider_.usePeakHold()) ? &peakHoldDb : nullptr);
  ```
  When toggled OFF you may also call `analyzerBridgeWidget_.clearPeakAndHoldTraces()` once on the
  transition so any cached CPU peak path clears immediately.

### 2f. Wire the param — `Source/ui/MainView.cpp`
Where the other trace params are read into `cfg` (~line 595), push the value to the view:
```cpp
analyzerView_.setShowPeak (getBoolParam ("analyzerShowPeak"));
```
(Do NOT add `showPeak` to the shared `TraceConfig` struct in melechdsp-hq — keep this AnalyzerPro-local
via the dedicated `showPeak_` member, so the SDK isn't touched.)

---

## Acceptance (human verifies in Pro Tools after Claude builds/signs/installs)
- Peak-hold (Hold engaged) sits at/above the live Peak and snaps up to new peaks instantly; live Peak
  no longer sails above it. With Hold off, no redundant duplicate hold line.
- New "Show Peak" toggle hides/shows the Peak (and its hold) without affecting channel/RMS traces.
  Default ON. Toggling channel traces no longer leaves an unwanted Peak overlay.
- No regression to RMS / channel traces / smoothness; no double-draw in chrome.
