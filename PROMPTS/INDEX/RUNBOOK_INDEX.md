# RUNBOOK INDEX — AnalyzerPro / MelechDSP

Maps maintenance themes to runbook prompts under `PROMPTS/RUNBOOKS/`.
Each runbook is a STOP-gated Cursor prompt (IMPLEMENTER + VERIFIER). Apply one phase at a time; review the diff before accepting.

---

## Display Performance / Rendering Cadence

### Steppy/juddery analyzer in VST3 & AAX (smooth in Standalone)
- `RUNBOOKS/ANALYZER_DISPLAY_SMOOTHNESS_V1.md`  [Phase 0 CLOSED 2026-05-31]
  - Phase 0 RESULT: NOT a cadence/render problem. VST3 == Standalone on all metrics. The reported difference was a STALE-BUILD comparison artifact (duplicate installs), resolved by the clean matched 1.1.1 install. Phase 1 (collapsing clocks) NOT pursued — unjustified by data.
  - See `MISSIONS/ANALYZER_DISPLAY_SMOOTHNESS_PHASE0_RESULT.md`.

### Glassy analyzer motion (absolute smoothness, all formats)
- `RUNBOOKS/ANALYZER_DISPLAY_GLASSY_MOTION_V2.md`  [active]
  - VBlank-driven render at display native rate (60/120Hz) + inter-frame interpolation of trace geometry between 30Hz data frames. Data pump stays 30Hz (no DSP cost).
  - STEP 2.0 prerequisite: gate COPY_PLUGIN_AFTER_BUILD so dev builds stop clobbering installs.
  - CPU-first: render-rate cap, idle-skip, kill-switch. Render-only; no DSP changes.

### Squared/flat-topped peaks when fractional-octave smoothing is on
- `RUNBOOKS/SPECTRUM_SMOOTHING_SQUARED_PEAKS_V1.md`  [open, PRE-EXISTING, not Phase 2]
  - Confirmed (A/B + code read): squaring is the UI convertFFTToLog resampling (flat box-average per log bin + coarse numLogBins grid + straight-segment render), active only when smoothing>0. Engine does not smooth (Option A).
  - Fix candidates: more log bins / Gaussian-weighted gather / smooth-curve render. Energy-conserving + RT-safe. Diagnose (STEP A) before fixing.

---

## Build / Release / Install

### Rebuild 1.1.1 (Release) + replace stale/duplicate installs
- `RUNBOOKS/REBUILD_AND_CLEAN_INSTALL_V1_1_1.md`
  - Fixes the "two builds, same version string" hazard (duplicate /Library + ~/Library installs).
  - Build (universal Release) → PACE-sign AAX → delete duplicates → install one canonical copy per format → verify version + build date.
  - Version is already wired to CMake via `scripts/plugin_version.sh` (do not hardcode).

### Pin the melechdsp-hq submodule WIP (before beta)
- `RUNBOOKS/SUBMODULE_PIN_MELECHDSP_HQ_V1.md`
  - Branch the detached-HEAD submodule, commit the glassy-motion + triangular-smoothing WIP, push to origin, bump the parent pointer.
  - CONFIRMED: shared/mdsp_dsp/{include,src}/dynamics MUST be committed (the build's CMakeLists compiles them); exclude cmake/cmake and dsp_bench.
  - Do this BEFORE RESTORE_SHIPPING_STATE so the beta build is reproducible.

### Restore shippable state after dev iteration
- `RUNBOOKS/RESTORE_SHIPPING_STATE_V1_1_1.md`
  - Run after the smoothness/Phase-2 work, before any beta hand-off.
  - Rebuilds SHIPPING (PLUGIN_DEV_MODE=OFF, no HUD), re-signs AAX (PACE, not adhoc), reinstalls one canonical signed copy per format incl. AU. Undoes the dev/HUD/adhoc/stale-AU state left by the investigation.

---

(Existing `.txt` runbooks under `PROMPTS/RUNBOOKS/` are not yet all catalogued here.)
