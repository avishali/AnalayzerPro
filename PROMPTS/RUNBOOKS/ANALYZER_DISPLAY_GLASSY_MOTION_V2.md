MISSION_ID: ANALYZER_DISPLAY_GLASSY_MOTION_V2

TITLE
Glassy analyzer motion in all formats: decouple render rate from data rate — drive repaint from the display's VBlank (60/120 Hz) and interpolate trace geometry between the 30 Hz data frames. Data/DSP cost unchanged.

WHY (Phase 0 result, 2026-05-31)
Phase 0 proved there is NO cadence/jitter problem and NO render-cost problem: VST3 and Standalone measured statistically identical (jitter ~1.5ms, paint ~2ms, ~29fps, zero throttling). The original "VST3 steppy / Standalone fine" was a stale-build comparison artifact, now resolved. So Phase 1 (collapsing clocks) is NOT pursued. The remaining limit on smoothness is fundamental: 30fps with no inter-frame interpolation, and ~29fps is a non-integer divisor of a 60/120Hz display → periodic 2-vs-3 refresh judder. The fix is to render at the display's native rate and tween between data frames.

GOAL
- Trace motion glides (no 30fps stepping) in Standalone, VST3, AU, AAX.
- Data pump stays 30 Hz. NO increase in FFT/DSP/audio-thread cost.
- Render is VBlank-aligned (no tearing/judder from non-integer cadence).
- CPU stays within budget (owner priority is CPU-first): paint_ms ~2ms today; render-rate increase is the only added cost — must be capped and gated.

HARD RULES
- No audio-thread / DSP / engine / snapshot changes. SnapshotPump stays 30 Hz.
- No allocations in paint() or in the per-VBlank interpolation hot path (preallocate display buffers; resize only on bin-count change, off the hot path).
- Interpolation is VISUAL ONLY (tweening already-ballistic-smoothed values). Do NOT add a second ballistics/smoothing stage.
- Peak-hold / peak markers must NOT be tweened in a way that lowers or lags a peak: peaks snap UP instantly; only the falling RMS/spectrum trace tweens.
- Minimal diffs. Build on the existing AAX VBlankAttachment path (AnalyzerDisplayView.cpp:198) — generalize, don't reinvent.
- Render-rate cap and a kill-switch must exist so we can A/B and ship conservatively.

FILES ALLOWED
- Source/ui/analyzer/AnalyzerDisplayView.cpp / .h
- third_party/melechdsp-hq: shared/mdsp_gui/src/analyzer/RTADisplay.cpp, include/.../RTADisplay.h
- third_party/melechdsp-hq: shared/mdsp_ui/.../rta/* ONLY if the trace-geometry build lives there (RTADisplayModel/Controller/Renderer) — keep diffs minimal.
- CMakeLists.txt (STEP 2.0 only)
NOTE: the submodule carries accepted uncommitted WIP (owner decision). Layer on top; do not revert it.

FILES FORBIDDEN
- AnalyzerEngine.*  AnalyzerSnapshot.*  PluginProcessor.*  any DSP/FFT compute code

============================================================
STEP 2.0 — PREREQUISITE: stop dev builds clobbering installs
============================================================
Phase 2 needs many dev builds. Today CMakeLists.txt:299 hardcodes COPY_PLUGIN_AFTER_BUILD TRUE, so every build overwrites the user/system installs (it already replaced the signed AAX with an adhoc one).
- Add a CMake option, default ON (preserve current shipping behavior):
    option(ANALYZERPRO_COPY_AFTER_BUILD "Copy built plugins to system folders after build" ON)
  and set COPY_PLUGIN_AFTER_BUILD ${ANALYZERPRO_COPY_AFTER_BUILD} in juce_add_plugin.
- Configure dev builds with -DANALYZERPRO_COPY_AFTER_BUILD=OFF so build-release-dev does NOT touch installs.
STOP and report the option + confirm a dev rebuild no longer overwrites ~/Library / Avid installs.

============================================================
IMPLEMENTER PROMPT (Phase 2 proper)
============================================================

ROLE
You are decoupling render from data and adding visual interpolation. Render-only.

STEP 2.1 — VBlank as the single render driver (all formats)
- Generalize the AAX-only VBlankAttachment (AnalyzerDisplayView.cpp:198) to ALL formats. On each VBlank: run the render tick (interpolate + repaint once).
- Keep the SnapshotPump at 30 Hz as the DATA source (unchanged). The 30 Hz juce::Timer no longer drives repaint directly.
- Ensure exactly ONE repaint per VBlank. Remove/neutralize redundant repaint() in RTADisplay data setters and RTADisplay's internal startTimerHz(30) (keep only non-data interaction work, if any).
STOP and report the render-driver wiring and which redundant repaints were removed.

STEP 2.2 — Double-buffer data frames for tweening
- On each new 30 Hz data frame (per trace: spectrum/band/log dB arrays), store it as "current" and move the old "current" to "previous", with a capture timestamp (juce::Time::getMillisecondCounterHiRes).
- Preallocate prev_, curr_, and display_ buffers; resize ONLY when bin/band count changes (off the hot path). No allocations on VBlank.
STOP and report the buffers + where frames are latched.

STEP 2.3 — Interpolate on VBlank
- On each VBlank: alpha = clamp((now - lastDataTimestamp) / dataIntervalMs, 0, 1), where dataIntervalMs ≈ 1000/30. Optionally clamp alpha slightly <1 or use a 1-frame render delay to avoid stalling at frame edges (document choice).
- display_[i] = lerp(prev_[i], curr_[i], alpha) for the RMS/spectrum/band trace.
- PEAKS: peak-hold and peak markers snap to the latest value (no downward tween); do not lerp peaks below curr_.
- Feed display_ to the existing path builder / renderer. Reuse existing path construction; only the input values change.
- ALSO FIX (regression from STEP 2.1): minDbAnim_ is reset with sample rate kAnalyzerDisplayTimerHz (30) at AnalyzerDisplayView.cpp ~155 and ~217, but getNextValue() now ticks at the VBlank rate (~60/120Hz) → the dB-range glide runs 2–4× too fast. Make the dB-range animation TIME-BASED (advance by measured wall-clock delta, reusing the same now/timestamp you compute for alpha) instead of assuming a fixed 30Hz call rate. Do NOT leave the 30Hz reset assumption.
STOP and report the interpolation math, alpha handling, peak treatment, and the dB-anim time-base fix.

STEP 2.4 — CPU guards (owner priority)
- Render-rate cap: add a config (e.g. kMaxRenderHz, default 60) so 120 Hz ProMotion displays don't paint 120×/s unless explicitly enabled. The cap MUST gate the marshaler DISPATCH (the VBlank → triggerAsyncUpdate / handleAsyncUpdate path), not just the repaint — otherwise analyzerUiTickCore (APVTS reads, applyPendingFftSizeIfNeeded, dB anim) also runs at full display rate. Capping the dispatch caps both tick logic AND paint.
- Cleanup: timerCallback() is now dead (the view's juce::Timer is never started after STEP 2.1). Either remove the Timer base/override or leave a clear comment; ensure no path relies on it. Confirm no -Wreorder warning from the constructor init-list change (navOverlay_).
- Idle skip: if data is unchanged AND alpha has reached 1.0 (trace fully settled / no motion), skip the repaint that frame. Resume on next data change.
  - WAKE COVERAGE (required): every user interaction that changes appearance but does NOT bump the data-frame serial must set forceNextRenderFrame_, or the change won't show while idle (transport stopped). Audit confirmed missing on: resetViewPeaks(), resetSessionMarker(), resetFrequencyView() — add forceNextRenderFrame_ = true to each (resetViewPeaks is a realistic "reset peaks while paused" workflow). Re-audit any other reset/toggle/weighting setters that neither kickSnapshotPumpImmediate nor set the force flag.
- Kill-switch: a compile-time or APVTS-independent flag to disable interpolation+vblank and fall back to the 30 Hz timer path (for A/B and conservative shipping).
- Verify NO allocations in paint() or the VBlank path (preallocated buffers only).
STOP and report the cap, idle-skip, kill-switch, and allocation audit.

STEP 2.5 — Measure + eye-check
- Build build-release-dev with -DANALYZERPRO_COPY_AFTER_BUILD=OFF -DPLUGIN_DEV_MODE=1.
- HUD read (VST3 + Standalone, steady audio, same window size):
  - paint/s should now track the display rate (≈60) while the trace is moving (capped per 2.4).
  - record paint_ms(last), and estimate CPU (Activity Monitor or DAW CPU meter) vs the 30 Hz baseline.
- Eye-check: motion should be visibly smoother/glassy with no stepping.
STOP and write PROMPTS/MISSIONS/ANALYZER_DISPLAY_GLASSY_MOTION_V2_RESULT.md:
- render-driver + interpolation summary
- before/after: paint/s, paint_ms, approx CPU
- eye-check verdict
- whether the cap/idle-skip kept CPU acceptable
End with STOP.

STEP 2.6 — Post-measurement fixes (found in STEP 2.5 owner measurement, 2026-06-01)
Fold into ONE commit. UI/render-only, no DSP.

2.6a — HUD jitter/expected artifact:
- timer_jitter_avg_ms and expect_timer_ms are computed against kAnalyzerDisplayTimerHz (30 → 33.33ms), but the tick now runs at the VBlank/render rate (~17ms/60Hz). This makes jitter read ~16ms (a false alarm) and expect_timer_ms misleading.
- FIX: base the diagnostics "expected interval" and jitter on the ACTUAL render cadence (e.g. derive expected from kMaxRenderHz or the measured VBlank period), and/or split the HUD into data_fps (30, the pump) vs render_fps (the VBlank tick) with jitter measured against the render period. Goal: jitter reflects real tick-to-tick deviation (should be small), not the 30-vs-60 mismatch.

2.6b — Idle-skip is INEFFECTIVE (primary CPU concern):
- Owner measured both Standalone and VST3 at IDLE (transport stopped, playback off) still painting ~60 fps (paint/s 78–80). Root cause: idle-skip triggers on dataFrameSerial_ (a NEW frame arriving), but the audio engine keeps calling processBlock when transport is stopped, so snapshots keep arriving and the serial keeps bumping → skip never fires.
- FIX: make idle-skip VALUE-BASED. In latchTraceFrame, compare the incoming values to the previous curr_ (now prev_); if the max abs delta across the trace is below an epsilon (e.g. ~0.05 dB), treat the frame as UNCHANGED. Track a "last actual change" timestamp/serial per active-mode trace (bump only on real change). 
  - A trace is "moving" only while (now - lastChangeMs) < kAnalyzerDataIntervalMs (so the tween from the last real change still completes).
  - shouldSkipRenderFrame uses the value-change signal (not raw frame arrival): skip when all active-mode traces are unchanged AND settled AND no dB-range/flash/peak-remap/force is active.
  - Peak-hold decay: if a decay is animating values, that counts as "changing" → keeps painting (correct). Infinite-hold static → idle → skip (correct).
- Expected result: at idle (static/silent signal), paint/s falls toward ~0; CPU drops. With moving audio, paint/s stays ~60 (glassy).
- Keep forceNextRenderFrame_ wake coverage from 2.4 intact.
- GAP found in 2.6 impl (501a68e): frameValueChanged covers only the main RMS + multi-trace buffers, NOT the peak/peak-hold trace. If peak-hold decay (Release control) outlasts the RMS release, RMS settles → idle-skip engages → peak decay FREEZES mid-fall. FIX: also factor peak-value change (fftPeakDbDisplay_/bandsPeakDbDisplay_/logPeakDbDisplay_ vs last rendered, or a peak-hold "is decaying" flag) into the not-idle condition. Verify by: play signal, stop transport, watch peak-hold fall smoothly to floor (must not freeze).

2.6c — Confirm paints bounded to the cap (optional, CPU hygiene):
- paint/s measured ~76–85 vs ~58 tick rate (~1.5×), implying extra/partial repaints beyond one-per-tick. Confirm the paint-timing callback isn't counting redundant FULL repaints; if there is a redundant full repaint per frame, remove it so paints are bounded to the render cap. Partial-region repaints (crosshair/markers) are acceptable.

Build dev. STOP and re-measure: idle paint/s should now drop toward 0; moving paint/s ~60; jitter realistic. Update the V2_RESULT file.

============================================================
VERIFIER PROMPT
============================================================

ROLE
Verify render/data decoupling + interpolation without DSP/CPU regressions.

CHECK 1 — Scope: only allowed files changed; no DSP/engine/snapshot/processor changes.
CHECK 2 — Data rate unchanged: SnapshotPump still 30 Hz; no FFT/DSP cost added.
CHECK 3 — Render decoupled: repaint driven by VBlank for ALL formats; exactly one repaint per VBlank (capped); redundant clocks/setters no longer repaint.
CHECK 4 — Interpolation correctness: RMS/spectrum tweens between prev/curr by alpha; peaks snap up and never tween downward/lag; no overshoot beyond [prev,curr].
CHECK 5 — No allocations in paint() or VBlank hot path.
CHECK 6 — CPU guards present and effective: render-rate cap, working kill-switch, AND idle-skip MUST be value-based (per STEP 2.6b). Measured proof required: with transport STOPPED / signal static, paint/s falls toward ~0 (NOT ~60). Serial-only idle-skip is a FAIL — the engine keeps publishing frames when stopped.
CHECK 7 — Runtime: motion visibly glassy in VST3 + Standalone; CPU within acceptable budget vs 30 Hz baseline; no tearing.

OUTPUT
Write PROMPTS/MISSIONS/ANALYZER_DISPLAY_GLASSY_MOTION_V2_VERIFIER_RESULT.md:
| Check | Status | Notes |
| Scope | PASS/FAIL | |
| Data rate unchanged | PASS/FAIL | |
| Render decoupled / 1 paint per vblank | PASS/FAIL | |
| Interpolation correctness | PASS/FAIL | |
| Paint/VBlank allocations | PASS/FAIL | |
| CPU guards | PASS/FAIL | |
| Runtime glassy + CPU budget | PASS/FAIL | |
End with STOP.
