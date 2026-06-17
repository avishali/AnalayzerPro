# Cursor task — Metal peak-hold from engine + un-disable-able last trace

Two changes. Both are about making the analyzer's Peak/Hold model clean and correct, matching how
the shared engine already works. Keep diffs tight. ARC OFF in `.mm` (don't release autoreleased
objects). One codebase — Mac (Metal) and Windows (CPU) must behave identically; the GPU path only
*renders* shared engine state, it must not own feature logic.

Decisions already made with the user:
- **Hold adds a frozen ceiling.** Peak = the moving live line. Engaging Hold ADDS a frozen
  session-max ceiling on top of it; Hold off = no ceiling. Hold must NEVER hide the Peak trace.
- **One trace stays un-disable-able.** The last enabled analyzer trace refuses to turn off, so the
  plot is never blank.

---

## Change A — Metal must RENDER the engine's peak-hold, not recompute it

**Why:** the shared engine already computes a correct peak-hold in
`AnalyzerSnapshot::fftPeakHoldDb` (instant attack; strict freeze when Hold on; HoldThenDecay when
off — see `third_party/melechdsp-hq/shared/mdsp_dsp/src/analyzer/AnalyzerEngine.cpp` `updatePeakHold`).
The CPU/Windows path renders it. The Metal pipeline currently throws it away and recomputes a hold
from the live peak/RMS — that recompute is what lagged before and what now blanks the Peak when Hold
engages. Fix = consume the engine field, exactly like the channel traces consume
`renderSnapshot.fftDb{L,R,Mid,Side,Mono}Rms`.

`renderSnapshot` is an `AnalyzerSnapshot` (filled by `analyzerEngine->getLatestSnapshot`) and already
exposes `fftPeakHoldDb` (the code reads `renderSnapshot.fftPeakDb` right next to it).

In `Source/ui/analyzer/metal/MetalHost.mm`, `updateAnalyzerPipelineFromSnapshot`:

1. **Reset branch (~line 1324):** change
   ```objcpp
   analyzerPeakHoldDb[i] = targetDb;
   ```
   to
   ```objcpp
   analyzerPeakHoldDb[i] = sanitizeAnalyzerDb (renderSnapshot.fftPeakHoldDb[i]);
   ```

2. **Main per-bin block (~lines 1339-1347):** replace the local hold recompute + decay
   ```objcpp
   const float currentPeakHoldDb = analyzerPeakHoldDb[i];
   const float decayedPeakHoldDb = renderSnapshot.isHoldOn
       ? currentPeakHoldDb
       : currentPeakHoldDb - peakDecayThisFrame;
   analyzerPeakHoldDb[i] = sanitizeAnalyzerDb (juce::jmax (analyzerPeakDb[i], decayedPeakHoldDb));

   const float rmsFloorDb = analyzerSmoothedDb[i];
   analyzerPeakDb[i] = sanitizeAnalyzerDb (juce::jmax (analyzerPeakDb[i], rmsFloorDb));
   analyzerPeakHoldDb[i] = sanitizeAnalyzerDb (juce::jmax (analyzerPeakHoldDb[i], rmsFloorDb));
   ```
   with a direct read of the engine hold (no second ballistics — the engine already did it):
   ```objcpp
   // Peak-hold is owned by the shared engine (instant attack; freeze when Hold on,
   // HoldThenDecay when off). Render it; do not recompute on the GPU side.
   analyzerPeakHoldDb[i] = sanitizeAnalyzerDb (renderSnapshot.fftPeakHoldDb[i]);

   const float rmsFloorDb = analyzerSmoothedDb[i];
   analyzerPeakDb[i] = sanitizeAnalyzerDb (juce::jmax (analyzerPeakDb[i], rmsFloorDb));
   ```
   Keep the existing `rmsAbovePeakViolations` telemetry check below if present (it still reads
   `analyzerPeakHoldDb[i]`/`rmsFloorDb` and stays valid).

3. **Remove now-dead decay locals (~lines 1210-1211):**
   ```objcpp
   const float peakDecayDbPerSecond = 60.0f / juce::jmax (0.01f, frame.rmsReleaseMs / 1000.0f);
   const float peakDecayThisFrame = peakDecayDbPerSecond * static_cast<float> (juce::jmax (0.0, renderDtSeconds));
   ```
   Delete both (their only use was the recompute just removed). If `renderDtSeconds` becomes unused
   in this function, `juce::ignoreUnused (renderDtSeconds);` — do NOT change the function signature.

**Visibility (already correct, just confirm):** in `Source/ui/analyzer/AnalyzerDisplayView.cpp` the
peak-hold trace visibility is `showPeak_ && isHoldOn_`. That already implements "Hold adds a frozen
ceiling; Hold off = no ceiling." Leave it. The live Peak trace stays gated on `showPeak_` only, so
engaging Hold can never hide it.

**Acceptance:** With Peak on + Hold on, a frozen session-max ceiling sits above the live Peak and
snaps up to new peaks instantly; the live Peak stays visible and moving. Hold off = no ceiling, Peak
unaffected. Matches the CPU/Windows look.

---

## Change B — keep at least one analyzer trace enabled (never-blank plot)

In `Source/ui/MainView.cpp`, `parameterChanged`, the trace-toggle branch (currently ~lines 564-576,
the `else if (parameterID == "TraceShowLR" || ... || "analyzerShowRMS" || "analyzerWeighting")`
block). Add a guard so the user can't turn off the LAST enabled trace. Do NOT apply the guard to
`analyzerWeighting` (it's not a trace toggle, it just shares this branch).

Replace the branch body with:
```cpp
{
    // Keep at least one analyzer trace enabled so the plot is never blank.
    // (analyzerWeighting is not a trace toggle — skip the guard for it.)
    if (parameterID != "analyzerWeighting" && newValue < 0.5f)
    {
        static const char* const kTraceShowIds[] = {
            "TraceShowLR", "analyzerShowMono", "analyzerShowL", "analyzerShowR",
            "analyzerShowMid", "analyzerShowSide", "analyzerShowPeak", "analyzerShowRMS"
        };
        bool anyOn = false;
        for (auto* id : kTraceShowIds)
            if (auto* v = apvts_->getRawParameterValue (id))
                if (v->load() > 0.5f) { anyOn = true; break; }

        if (! anyOn)
        {
            // Veto: re-enable the trace the user just switched off. The re-set re-enters
            // parameterChanged with value 1.0 and syncs normally (no infinite loop).
            if (auto* p = apvts_->getParameter (parameterID))
                p->setValueNotifyingHost (1.0f);
            return;
        }
    }

    juce::ignoreUnused (newValue);
    syncAnalyzerTraceConfig();
}
```
`getRawParameterValue` reflects the already-applied change when `parameterChanged` fires, so when the
user turns off the last trace all eight read false → veto. The veto write flips the bound toggle
button back ON via the existing ControlBinder, so the button never shows a misleading off state.

**Acceptance:** Turning off traces one by one stops at the last one — it won't switch off, the plot
keeps showing it, and its toggle button stays lit. No fully empty plot.

---

## Build/verify (human runs sign+install; Claude builds + verifies)
- `cmake --build build-debug --target AnalyzerPro_Standalone` must succeed; `git diff --check` clean.
- Then Claude: reconfigure → build release AAX → PACE sign → mirror-install → installed SHA == signed
  SHA (must differ from `8114432ba481`) → PT verify both acceptance checks.
