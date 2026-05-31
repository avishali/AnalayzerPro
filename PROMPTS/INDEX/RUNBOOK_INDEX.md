# RUNBOOK INDEX — AnalyzerPro / MelechDSP

Maps maintenance themes to runbook prompts under `PROMPTS/RUNBOOKS/`.
Each runbook is a STOP-gated Cursor prompt (IMPLEMENTER + VERIFIER). Apply one phase at a time; review the diff before accepting.

---

## Display Performance / Rendering Cadence

### Steppy/juddery analyzer in VST3 & AAX (smooth in Standalone)
- `RUNBOOKS/ANALYZER_DISPLAY_SMOOTHNESS_V1.md`
  - Root cause: three independent 30 Hz timers + per-setter repaint() → irregular in-host cadence.
  - Phase 0 = instrument & measure (gate). Phase 1 = single vsync-aligned cadence. Phase 2 (optional, needs sign-off) = render/data decoupling + interpolation.
  - Render-only; no DSP/data-rate increase. CPU-first.

---

## Build / Release / Install

### Rebuild 1.1.1 (Release) + replace stale/duplicate installs
- `RUNBOOKS/REBUILD_AND_CLEAN_INSTALL_V1_1_1.md`
  - Fixes the "two builds, same version string" hazard (duplicate /Library + ~/Library installs).
  - Build (universal Release) → PACE-sign AAX → delete duplicates → install one canonical copy per format → verify version + build date.
  - Version is already wired to CMake via `scripts/plugin_version.sh` (do not hardcode).

---

(Existing `.txt` runbooks under `PROMPTS/RUNBOOKS/` are not yet all catalogued here.)
