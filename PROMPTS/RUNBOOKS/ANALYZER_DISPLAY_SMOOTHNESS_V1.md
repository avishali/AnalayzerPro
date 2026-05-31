MISSION_ID: ANALYZER_DISPLAY_SMOOTHNESS_V1

TITLE
Fix steppy / juddery analyzer display in VST3 & AAX (smooth in Standalone) by collapsing redundant repaint clocks to a single vsync-aligned cadence. Render-only; no DSP, no extra data/CPU.

SYMPTOM (reported by repo owner)
- Motion looks "steppy / stuttery" — low-frame-rate feel — in VST3 and AAX.
- Standalone looks fine.
- Same nominal 30 Hz everywhere.
- Priority: CPU headroom first (especially Pro Tools message thread). Do NOT raise data rate or DSP cost to buy smoothness.

ROOT-CAUSE HYPOTHESIS (confirm in Phase 0 before fixing)
There are THREE independent 30 Hz timers, each able to trigger repaint(), plus every RTADisplay::setXXXData setter calls repaint() directly:
  1. SnapshotPump timer — third_party/melechdsp-hq/shared/mdsp_gui/include/mdsp_gui/common/SnapshotPump.h (startTimerHz) → onSnapshot → AnalyzerDisplayView::updateFromSnapshot → setFFTData → repaint()
  2. RTADisplay's own timer — third_party/melechdsp-hq/shared/mdsp_gui/src/analyzer/RTADisplay.cpp:35 (startTimerHz(30)) → timerCallback → controller_.onTimerTick()
  3. AnalyzerDisplayView timer — Source/ui/analyzer/AnalyzerDisplayView.cpp:217 (startTimerHz)
  + RTADisplay setters call repaint() at lines ~49, 72, 78, 87, 105, 118.
In Standalone the message thread is idle so these coalesce cleanly. In a busy host (VST3/AAX) they fire at irregular phases relative to each other and to vsync → JUCE coalesces unpredictably → painted frames land at irregular intervals → perceived stutter. (Note: the recently-dropped UI ballistics layer is amplitude smoothing, NOT a frame issue; it is orthogonal — do not rely on it for this fix.)

HARD RULES
- No audio-thread / DSP / engine / snapshot changes.
- Do NOT increase the data pump rate or FFT cost. Data stays 30 Hz.
- No allocations in paint().
- Minimal diffs. Render cadence plumbing only.
- Net repaint count per produced frame must go DOWN, not up (CPU-first).
- Measure (Phase 0) before changing cadence (Phase 1). Phase 1 is conditional on Phase 0 confirming cadence/jitter — not paint cost — is the bottleneck.

FILES ALLOWED
- Source/ui/analyzer/AnalyzerDisplayView.cpp / .h
- third_party/melechdsp-hq/shared/mdsp_gui/src/analyzer/RTADisplay.cpp
- third_party/melechdsp-hq/shared/mdsp_gui/include/mdsp_gui/analyzer/RTADisplay.h
- (read-only reference) SnapshotPump.h, AnalyzerDisplayWidget.h

FILES FORBIDDEN
- AnalyzerEngine.*  AnalyzerSnapshot.*  PluginProcessor.*  any DSP/FFT compute code

============================================================
PHASE 0 — INSTRUMENT & MEASURE (do this first, then STOP for owner review)
============================================================

ROLE
You are adding measurement only. No cadence changes yet.

STEP 0.1 — Enable the existing dev HUD for VST3 (not just AAX)  [DONE 2026-05-31, with one required amendment below]
There is already a diagnostics line at Source/ui/analyzer/AnalyzerDisplayView.cpp ~805 (devModeDebugLine_) exposing: target_fps, actual_fps, actual_timer_ms_avg, timer_late_cnt/s, timer_jitter_avg_ms, paint_ms(last), paint/s, pump_throttle/s, pump_reject/s. It was gated to the AAX build path.
- Make these counters and the HUD line available for VST3 (and Standalone, for the baseline comparison).  [done: now under ANALYZERPRO_DEV_DIAGNOSTICS, counters renamed aaxDiag*→uiDiag*, format name printed dynamically, paint-timing callback installed for all diagnostics builds]
- Add a per-frame paint counter if one is not already derivable, so paint/s is meaningful in all formats.  [done: paint-timing callback feeds uiDiagPaintEventsAccum_]

REQUIRED AMENDMENT (owner decision 2026-05-31) — gate on PLUGIN_DEV_MODE ALONE, not JUCE_DEBUG:
- Current gate is `ANALYZERPRO_DEV_DIAGNOSTICS = JUCE_DEBUG && PLUGIN_DEV_MODE`. Change it to depend on PLUGIN_DEV_MODE only (drop the JUCE_DEBUG requirement).
- WHY: measurements must run on an optimized RELEASE build to be perf-honest. Requiring JUCE_DEBUG forces a Debug build (skewed timing) and is a regression vs the old AAX path which printed the HUD in Release when PLUGIN_DEV_MODE=1.
- SAFETY: Release still ships clean because scripts/build_release.sh passes -DPLUGIN_DEV_MODE=OFF (→ PLUGIN_DEV_MODE=0 compile def, CMakeLists.txt:410). The fallback default in the header/DevFlags.h is 0. So no Release leakage.
- To MEASURE: configure a Release build with -DPLUGIN_DEV_MODE=1 (HUD on, optimized). Keep build_release.sh's default OFF for shipping.
STOP and report the final gate expression and how to enable the HUD on a Release build.

STEP 0.1c — Baseline hygiene: commit in separate, clean commits (owner decision 2026-05-31)
The working tree currently mixes two unrelated changes in AnalyzerDisplayView.cpp. Separate them so the measurement baseline is unambiguous. Use `git add -p` / `git stash -p` to split hunks:
  Commit A (pre-existing WIP): AAX kAnalyzerUiFps 15→30 + removal of UI-side applyBallistics() in updateFromSnapshot (+ their comment edits).
    msg: "AAX UI cadence 15→30 Hz; drop redundant UI ballistics (engine owns ballistics)"
  Commit B (Phase 0.1 instrumentation): ANALYZERPRO_DEV_DIAGNOSTICS gating (PLUGIN_DEV_MODE-only per amendment), aaxDiag*→uiDiag* rename, dynamic format name, paint-timing callback generalization, PLUGIN_DEV_MODE header fallback 1→0.
    msg: "Analyzer: enable dev diagnostics HUD for all formats (PLUGIN_DEV_MODE-gated)"
Do NOT mix the two. The scripts/* and PROMPTS/* changes are unrelated tooling — commit or leave as the owner prefers, but keep them out of A and B.
STOP and report the two commit hashes.

STEP 0.1d — Instrumentation correctness fixes (found in first measurement, 2026-05-31)
First measurement exposed two HUD bugs that must be fixed before numbers are usable. UI-only, render path only.
  BUG 1 — Format label always prints "AAX": the HUD uses `#if JucePlugin_Build_AAX/VST3/Standalone`, but those macros are compile-time and TRUE for every format the project builds, so the chain always resolves to AAX in all binaries. The label is meaningless.
    FIX: derive the label at runtime from the wrapper:
      const juce::String formatName = juce::AudioProcessor::getWrapperTypeDescription (audioProcessor.wrapperType);
    Use that in devModeDebugLine_. (Returns "VST3"/"AU"/"AAX"/"Standalone" per instance.)
  BUG 2 — HUD line truncated: drawn at AnalyzerDisplayView.cpp ~430 as g.drawText(devModeDebugLine_, 8, 38, 700, 14, ...) — fixed 700px single line, no wrap. The decisive fields (timer_jitter_avg_ms, paint_ms(last), paint/s, pump_throttle/reject) are clipped off-screen.
    FIX: render without truncation — e.g. g.drawFittedText (devModeDebugLine_, 8, 38, getWidth() - 16, 48, juce::Justification::topLeft, 3) so it wraps to up to 3 lines at full width. Additionally reorder the line so the discriminating metrics (paint/s, paint_ms(last), timer_jitter_avg_ms) come FIRST, before target_fps/expect_timer_ms/scale.
  Commit as a fixup to commit B's area. msg: "Analyzer HUD: runtime wrapper label + non-truncated multi-line draw"
STOP and report the fix + commit hash. Then re-measure (STEP 0.2).

STEP 0.2 — Capture numbers (owner will run, or you run if permitted)
MEASUREMENT BUILD (required): the HUD is gated on PLUGIN_DEV_MODE only, and scripts/build_release.sh forces -DPLUGIN_DEV_MODE=OFF (shipping stays clean). So produce a SEPARATE optimized build with the HUD on — do NOT reuse the shipping build_release.sh output for measurement:
    cmake -S . -B build-release-dev -DCMAKE_BUILD_TYPE=Release \
      -DJUCE_PATH=/Users/avishaylidani/DEV/SDK/JUCE \
      -DAAX_SDK_PATH=/Users/avishaylidani/Downloads/aax-sdk-2-8-0 \
      -DUniversalBinary=ON -DPLUGIN_DEV_MODE=1 -DCMAKE_OSX_DEPLOYMENT_TARGET=10.13
    cmake --build build-release-dev --config Release -j
This is Release-optimized (perf-honest) AND has the HUD. Keep it OUT of installer/signing scripts — it is a measurement-only build. Load these VST3/Standalone artefacts directly for the read.
With audio running, record for Standalone vs VST3 (and AAX if available):
- actual_fps, timer_jitter_avg_ms, paint_ms(last), paint/s, pump_throttle/reject.
STOP and write PROMPTS/MISSIONS/ANALYZER_DISPLAY_SMOOTHNESS_PHASE0_RESULT.md with the measurements and your read:
- Is in-host paint/s irregular or below 30, and/or jitter high? (→ cadence problem → Phase 1)
- OR is paint_ms the bottleneck? (→ render-cost problem → different fix; STOP and escalate)
End with STOP. Do NOT start Phase 1 until the owner confirms cadence is the issue.

============================================================
PHASE 1 — SINGLE CADENCE SOURCE (only after Phase 0 confirms cadence/jitter)
============================================================

ROLE
You are collapsing the repaint clocks to one vsync-aligned source. Fewer paints, regular cadence.

STEP 1.1 — Make RTADisplay setters dirty-flag instead of repaint
In RTADisplay (third_party hq):
- Add bool dirty_ = false;
- In setFFTData / setBandData / setLogData / setFftMeta / setBandCenters / setDbRange / setFrequencyRange: update model/controller and set dirty_ = true. REMOVE the direct repaint() calls in these setters.
STOP and report the setters changed.

STEP 1.2 — One coalesced repaint per frame
- Drive a single tick that, when dirty_ (or when animating, e.g. dB-range glide), issues exactly one repaint() and clears dirty_.
- Prefer driving this tick from juce::VBlankAttachment so paints are phase-locked to display refresh (the AAX path already uses VBlankAttachment at Source/ui/analyzer/AnalyzerDisplayView.cpp:198 — generalize that approach to VST3/AU instead of startTimerHz).
- Keep the SnapshotPump at 30 Hz unchanged (data rate unchanged).
STOP and report the single tick path and how dirty_ is consumed.

STEP 1.3 — Remove the redundant clock(s)
- Remove RTADisplay's internal startTimerHz(30) repaint driver (RTADisplay.cpp:35) IF onTimerTick only repaints. If onTimerTick does interaction/animation work unrelated to data, keep that work but ensure it does not issue its own data-frame repaint.
- Ensure AnalyzerDisplayView's timer no longer double-drives repaint for data frames.
STOP and report which timers were removed/kept and why.

STEP 1.4 — Verify one paint per frame
Using the Phase 0 HUD, confirm paint/s is now steady and equals (not exceeds) the produced-frame rate; timer_jitter is low; CPU same or lower than before.
STOP and write PROMPTS/MISSIONS/ANALYZER_DISPLAY_SMOOTHNESS_PHASE1_RESULT.md (files changed, cadence path, before/after paint/s + jitter + CPU). End with STOP.

(OPTIONAL PHASE 2 — only if Phase 1 is still not smooth: decouple render rate from data rate — render at vblank ~60 Hz and interpolate trace geometry between the two most recent 30 Hz data frames. This adds paint cost, so it requires explicit owner sign-off given the CPU-first priority. Do NOT implement without approval.)

============================================================
VERIFIER PROMPT
============================================================

ROLE
Verify cadence was unified without raising data/DSP cost.

CHECK 1 — Scope lock: only allowed files modified; no DSP/engine/snapshot/processor changes.
CHECK 2 — Data rate unchanged: SnapshotPump still 30 Hz; no FFT/DSP cost added.
CHECK 3 — Fewer paints: net repaint() calls per produced frame decreased; setters no longer repaint directly.
CHECK 4 — Single cadence: exactly one coalesced repaint per frame; ideally VBlank-driven for VST3/AU.
CHECK 5 — No allocations in paint().
CHECK 6 — Runtime: VST3/AAX motion smooth, paint/s steady, jitter low; Standalone unchanged; CPU same or lower.

OUTPUT
Write PROMPTS/MISSIONS/ANALYZER_DISPLAY_SMOOTHNESS_VERIFIER_RESULT.md:
| Check | Status | Notes |
| Scope lock | PASS/FAIL | |
| Data rate unchanged | PASS/FAIL | |
| Fewer paints | PASS/FAIL | |
| Single cadence | PASS/FAIL | |
| Paint allocation | PASS/FAIL | |
| Runtime smoothness/CPU | PASS/FAIL | |
End with STOP.
