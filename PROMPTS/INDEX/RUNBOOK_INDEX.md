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

---

## Build / Release / Install

### Rebuild 1.1.1 (Release) + replace stale/duplicate installs
- `RUNBOOKS/REBUILD_AND_CLEAN_INSTALL_V1_1_1.md`
  - Fixes the "two builds, same version string" hazard (duplicate /Library + ~/Library installs).
  - Build (universal Release) → PACE-sign AAX → delete duplicates → install one canonical copy per format → verify version + build date.
  - Version is already wired to CMake via `scripts/plugin_version.sh` (do not hardcode).

### Restore shippable state after dev iteration
- `RUNBOOKS/RESTORE_SHIPPING_STATE_V1_1_1.md`
  - Run after the smoothness/Phase-2 work, before any beta hand-off.
  - Rebuilds SHIPPING (PLUGIN_DEV_MODE=OFF, no HUD), re-signs AAX (PACE, not adhoc), reinstalls one canonical signed copy per format incl. AU. Undoes the dev/HUD/adhoc/stale-AU state left by the investigation.

---

(Existing `.txt` runbooks under `PROMPTS/RUNBOOKS/` are not yet all catalogued here.)
