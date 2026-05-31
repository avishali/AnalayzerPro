# ANALYZER_DISPLAY_GLASSY_MOTION_V2_RESULT

Date: 2026-06-01

## Commits

- STEP 2.2 + STEP 2.3: `a449353` — `Analyzer: VBlank inter-frame interpolation of trace geometry + time-based dB-range anim`
- STEP 2.4: `aa35dfb` — `Analyzer: VBlank render-rate cap, idle-skip with serial/force wake, interpolation kill-switch`

Submodule changes were not staged for either commit.

## STEP 2.4 Completion

- Added `ANALYZERPRO_MAX_RENDER_HZ`, defaulting to `60`, and gated the VBlank lambda before `triggerAsyncUpdate()`.
- Added idle-skip using `dataFrameSerial_`, `renderedDataFrameSerial_`, per-mode trace motion checks, and `forceNextRenderFrame_`.
- Added `ANALYZERPRO_USE_VBLANK_INTERPOLATION`, defaulting to enabled, with a fallback to the 30 Hz `juce::Timer` path.
- Scoped interpolation work to the active mode only.
- Retired the timer path from normal operation and documented it as fallback-only.
- Added force-wake coverage for reset/toggle paths that can change appearance while transport is stopped:
  - `resetSessionMarker()`
  - `resetViewPeaks()`
  - `resetFrequencyView()`
  - `setSpectrumFftOrder()`
  - `setSpectrumDecayRate()`
  - Existing force wakes remain in dB range, peak range, mode, gain, tilt, trace config, and frequency view changes.

## Allocation Audit

- VBlank dispatch path only checks flags/timestamps and calls `triggerAsyncUpdate()` when needed.
- Per-VBlank interpolation writes into pre-sized `display_` vectors.
- Peak remapping in the VBlank tick no longer resizes display peak vectors; it only updates them when sizes already match.
- Remaining `std::vector::resize()` calls are confined to snapshot/data-prep or structural count-change paths, not steady-state VBlank interpolation.

## Build Verification

Command:

```sh
cmake -S . -B build-release-dev -DCMAKE_BUILD_TYPE=Release -DPLUGIN_DEV_MODE=1 -DANALYZERPRO_COPY_AFTER_BUILD=OFF
cmake --build build-release-dev --config Release -j 8
```

Result: passed.

Artifacts:

- Standalone: `build-release-dev/AnalyzerPro_artefacts/Release/Standalone/AnalyzerPro.app`
- VST3: `build-release-dev/AnalyzerPro_artefacts/Release/VST3/AnalyzerPro.vst3`
- AAX: `build-release-dev/AnalyzerPro_artefacts/Release/AAX/AnalyzerPro.aaxplugin`

Build log check:

- No `-Wreorder` warning found.
- Warnings observed were pre-existing/shared warnings such as `FloatingIconPanel::hitTest` hiding `juce::Component::hitTest`, `ui_` unused, and JUCE/APVTS deprecation warnings.

## STEP 2.5 Measurement Status

Standalone launch:

- Launched `build-release-dev/AnalyzerPro_artefacts/Release/Standalone/AnalyzerPro.app`.
- Process was alive after launch.
- A shell CPU sample while the GUI could not be inspected reported approximately `35-50%` for the standalone process; this is not a valid analyzer baseline comparison because the agent could not confirm window visibility, audio state, or transport state.

HUD / eye-check:

- Blocked in this agent environment. `screencapture` failed with `could not create image from display`.
- `System Events` could see the process but could not access a front window for sizing/capture.
- Because the GUI could not be captured, the HUD values for `paint/s`, `paint_ms(last)`, `data_fps`, and jitter were not available.

VST3 load:

- Blocked in this agent environment. No VST3 host or `pluginval` executable was available.
- The VST3 artifact was built successfully, but the GUI load and HUD capture could not be performed here.

## Required Manual Eye-Check

Run on a desktop session with screen capture and a VST3 host available:

1. Load the release-dev Standalone and VST3 at the same window size.
2. Feed steady audio.
3. Capture the dev HUD while traces are moving and again after transport/audio stops.
4. Confirm:
   - Moving trace: `paint/s` tracks the capped display rate, approximately `60`.
   - Idle trace: `paint/s` drops when alpha has settled and no animation is active.
   - `data_fps` remains `30`.
   - Motion is visibly glassy with no obvious 30 fps stepping.
   - CPU delta versus the 30 Hz baseline is acceptable.

## STEP 2.5 — Owner-captured HUD (2026-06-01, dev build, playing audio)

| metric | VST3 (DAW) | Standalone | Phase 0 baseline |
|---|---|---|---|
| tick | VBlank | VBlank | Timer |
| actual_fps | 57.6–58.5 | 58.0–59.5 | ~29 |
| data_fps | 30 | 30 | 30 |
| paint_ms(last) | 0.90–1.86 | 0.41–1.22 | ~1.8–2.8 |
| paint/s | 77–84 | 76–85 | 29–30 |
| pump throttle/reject | 0 / 0 | 0 / 0 | 0 / 0 |

VERDICT: Phase 2 core goal MET. Render decoupled to VBlank ~60 fps (2× the old ~29) while data stays 30 fps (no DSP cost); paint ~1 ms.

Notes / known artifacts (not regressions):
1. timer_jitter_avg_ms≈16 is a HUD metric artifact: expect_timer_ms is still hardcoded to 33.33 (kAnalyzerDisplayTimerHz=30) while the tick now runs at ~17 ms (60 Hz). Real cadence is steady (~17 ms consistently). FIX: base the jitter "expected" on the actual render interval, or label it data-rate vs render-rate explicitly.
2. paint/s (76–85) exceeds the ~58 tick rate (~1.5×) — likely multi-pass/partial repaints counted by the paint-timing callback. paint_ms is tiny so CPU impact is small, but paints are not hard-bounded to the 60 Hz cap. Worth confirming no redundant full repaints if strict capping matters.
3. Idle-skip keys off "no new data frame" (transport stopped), not "signal silent." A running-but-silent input still latches 30 Hz frames → still paints at 60. To verify idle-skip, STOP the transport and confirm paint/s falls toward 0.

OUTSTANDING before close:
- Owner eye-check: confirm motion is visibly glassy vs the old 30 fps stepping, and the ~33 ms interpolation latency is not sluggish.
- Idle-skip confirmation: stop transport, confirm paint/s → ~0.
- Optional: real CPU delta (Activity Monitor) playing vs idle.

