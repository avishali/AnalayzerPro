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

## UI/UX Overhaul (staged program — Cursor solo, owner supervises)
- `RUNBOOKS/UI_OVERHAUL/00_MASTER_PLAN.md` — control inventory, proposed IA, stage list + sequencing + solo-safety. READ FIRST.
- `01_FREQ_RANGE_AND_DETAIL.md` — 10 Hz→Nyquist range, larger FFT sizes, labeled Detail control. (concrete; do first)
- `05_PANEL_RESIZE_LOCK.md` — lock placement + resize stability. (concrete; foundational, before 04)
- `04_PHASE_SCOPE.md` — phase-arc scope visuals + resize. (after 05)
- `06_METERS_FINISH.md` — meters final UI, reuse MasterLimiter. (concrete)
- `03_LOOKFEEL_GENERALIZE.md` — generalize MasterLimiterLookAndFeel → shared mdsp_ui. (review-gated)
- `02_CONTROL_IA_REORG.md` — dedupe rail/popup, module-context rail, surface all controls incl. Side. (review-gated; do last)
- Locked decisions: 10 Hz→Nyquist · FFT-size + labeled Detail · design generalized into mdsp_ui · subjective stages = Cursor builds proposed IA, owner reviews.

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

### Send a beta — signed + notarized .pkg (works on all macOS)
- `RUNBOOKS/SEND_BETA_NOTARIZED_PKG_V1.md`
  - `SIGN_AND_NOTARIZE_SKIP_AAX=1 ./scripts/sign_and_notarize.sh` → Developer-ID-sign (keep PACE AAX) → pkg → notarize → staple.
  - Certs + AC_PASSWORD notarytool profile already set up (team C5UC779LGC). Deliverable: installer/AnalyzerPro-1.1.1-macOS-signed.pkg.
  - Notarized+stapled = no Gatekeeper dance on macOS 15/older; supersedes the old unsigned "simple installer".

### Restore shippable state after dev iteration
- `RUNBOOKS/RESTORE_SHIPPING_STATE_V1_1_1.md`
  - Run after the smoothness/Phase-2 work, before any beta hand-off.
  - Rebuilds SHIPPING (PLUGIN_DEV_MODE=OFF, no HUD), re-signs AAX (PACE, not adhoc), reinstalls one canonical signed copy per format incl. AU. Undoes the dev/HUD/adhoc/stale-AU state left by the investigation.

---

(Existing `.txt` runbooks under `PROMPTS/RUNBOOKS/` are not yet all catalogued here.)
