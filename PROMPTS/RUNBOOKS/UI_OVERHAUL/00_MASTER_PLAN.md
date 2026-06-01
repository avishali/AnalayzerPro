# AnalyzerPro UI/UX Overhaul — MASTER PLAN

Date: 2026-06-01. Owner: Avishay. Mode: Cursor executes solo (owner supervises visually); final revision with the architect later.
Decisions locked (2026-06-01): freq range 10 Hz → Nyquist · detail = FFT-size + a labeled detail concept · design generalized into shared mdsp_ui · for subjective stages Cursor builds the proposed IA and owner reviews visually.

Each stage has its own runbook in this folder (NN_*.md). Apply ONE stage at a time, STOP-gated, owner eye-checks before moving on. RT/DSP rules from PROMPTS/README.txt still apply. Submodule melechdsp-hq is pinned at b2983a9 (branch analyzerpro/glassy-motion-and-smoothing) — commit shared-lib changes there and bump the parent pointer.

---

## CONTROL INVENTORY (current state)

Surfaces:
- HeaderBar (top): module tabs (Spectrum / Scopes / Meters / Traces), Bypass, Preset, Save, A/B, dB-range readout, freq zoom +/-/reset, rail toggle.
- ControlRail (side, collapsible sections Scopes/Traces/Analysis Mode): FFT Size, Hold, Tilt, Scope Mode, Scope Shape, Scope Input, Scope Hold, Meter Input, Meter Hold, Show Stereo/Mono/Left/Right/Mid/Side/RMS, Smoothing, Weighting; FFT/BAND/LOG mode buttons.
- SettingsPopupPanel: **DUPLICATES** the rail — FFT Size, Tilt, Smoothing, Weighting, Hold Peaks, Reset, Scope Mode/Shape/Input/Hold, Meter Input/Hold, Show Stereo/Mono/L/R/Mid/Side/RMS, FFT/BAND/LOG.
- FooterBar: status text, Hold toggle, Release time.
- Panels: RTADisplay (spectrum), StereoScope/PAZ phase arc, meters (L/R in/out, LUFS momentary/short/integrated/peak), loudness numeric.

KEY PROBLEMS:
1. ControlRail and SettingsPopupPanel carry near-identical controls (two sources of truth → confusion, drift, the "where is what" feeling).
2. Controls are not grouped by the module they affect; everything is flat.
3. Some controls are present in code but hard to reach/surface (e.g. Show Side — exists as showSideButton/sideBtn_, but the surfacing is unreliable; see the separate Side-trace task).
4. No clear visual hierarchy / the look predates the MasterLimiter design pass.

---

## PROPOSED CONTROL IA (target — owner reviews visually)

Single source of truth, organized by the 4 modules the HeaderBar already implies.

- HEADER (global, always visible): Module tabs [Spectrum | Scopes | Meters | Traces] · Bypass · A/B · Preset · Save · global view controls (dB range, freq zoom/reset).
- SIDE RAIL (context-sensitive to the active module tab; collapsible). The rail shows ONLY the active module's controls:
  - Spectrum: FFT Size · Detail · Smoothing · Weighting · Tilt · Freq Range/Zoom · Hold/Reset.
  - Scopes: Scope Mode · Shape · Input · Hold.
  - Meters: Meter Input · Hold · (loudness/meter config).
  - Traces: Show Stereo / Mono / Left / Right / Mid / Side / RMS · Peak · Hold (the trace on/off cluster, with the Side toggle reliably visible).
- FOOTER (global): status · Hold · Release.
- ELIMINATE the duplication: SettingsPopupPanel and ControlRail must not both own the same controls. Decision: the RAIL is the single source of truth (context-sensitive). Either remove SettingsPopupPanel or repurpose it as a read-only "all settings" overview — NOT a second editable copy. (Owner to confirm in Stage 2.)

This IA is the spec for Stage 2. It is a proposal — owner adjusts after seeing it.

---

## STAGES (sequence + solo-safety)

| # | Stage | Runbook | Solo-safe? | Depends on |
|---|-------|---------|-----------|------------|
| 1 | Extended freq range (10 Hz→Nyquist) + detail control | 01_FREQ_RANGE_AND_DETAIL.md | Mostly (concrete spec; owner eye-checks LF detail) | engine RT-safe FFT resize (exists) |
| 2 | Control IA reorganization (dedupe, module-context, surface all) | 02_CONTROL_IA_REORG.md | Review-gated (subjective) | Stage 1 controls; IA spec above |
| 3 | Look & feel: generalize MasterLimiterLookAndFeel → mdsp_ui | 03_LOOKFEEL_GENERALIZE.md | Review-gated (subjective) | shared lib (submodule) |
| 4 | Phase arc scope: visuals + panel resize | 04_PHASE_SCOPE.md | Mostly | Stage 5 (resize stability) |
| 5 | Panel placement lock + resize stability | 05_PANEL_RESIZE_LOCK.md | Yes (concrete) | — |
| 6 | Meters: final UI touches (reuse ML meter work) | 06_METERS_FINISH.md | Mostly | Stage 3 (look) helps |
| 7 | Surface the missing Side-trace control | (see spawned task) | Yes | overlaps Stage 2 |

RECOMMENDED SOLO ORDER while architect is away:
1) Stage 1 (freq range + detail) — high-value, self-contained, concrete win.
2) Stage 5 (resize/lock stability) — foundational; fixes a class of bugs and unblocks Stage 4.
3) Stage 4 (phase scope) — builds on 5.
4) Stage 6 (meters) — concrete polish.
5) Stage 3 (look & feel generalization) — shared design; review-gated.
6) Stage 2 (control IA reorg) — biggest subjective change; do last, owner reviews carefully.

Rationale: front-load the concrete/solo-safe stages (1,5,4,6); leave the two subjective stages (3,2) where the owner's eye matters most, and where the architect's final revision adds the most value.

---

## GUARDRAILS FOR SOLO EXECUTION
- One stage per session; STOP at each runbook checkpoint; owner eye-checks before continuing.
- Render-only stages must not touch DSP/engine/snapshot. Engine/DSP stages stay RT-safe (no audio-thread allocations/locks).
- Shared-lib (mdsp_ui / mdsp_dsp) changes: commit in the submodule on the pinned branch, push, bump the parent pointer (see SUBMODULE_PIN runbook for the exact dance).
- After each stage, rebuild a dev build (-DPLUGIN_DEV_MODE=1 -DANALYZERPRO_COPY_AFTER_BUILD=OFF) and load Standalone for the eye-check; do NOT clobber the signed beta install.
- Anything ambiguous or subjective beyond the proposed IA → STOP and leave a note in the stage RESULT file for architect review rather than guessing.
