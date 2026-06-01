MISSION_ID: UI_OVERHAUL_04_PHASE_SCOPE

TITLE
Improve the phase-arc (stereo) scope: better visuals and clean panel resize. Do AFTER Stage 5 (resize stability).

GOAL
- The phase/goniometer/PAZ arc reads clearly: smooth anti-aliased trace, correct scaling that fills the panel and scales correctly on resize (a prior pass touched this — finish it), readable grid/labels (L / R / Mono / Anti-phase), tasteful fill/persistence.
- Panel resizes cleanly with the rest of the layout (no stretch distortion, correct aspect for the arc).

LIKELY FILES
- Source/ui/analyzer/StereoScopeView.cpp/.h, Source/ui/scopes/* if present, third_party/.../mdsp_ui/.../scopes/* (shared scope renderer), and the scope-shape control (Basic/PAZ in ControlRail/SettingsPopup).
- Scope DSP (StereoScopeAnalyzer) is in the engine — DO NOT change DSP; this is render/scaling only.

HARD RULES
- Render-only. The scope's data (StereoScopeAnalyzer pushSamples) is engine-side; don't touch it.
- Honor the resize-stability rules from Stage 5 (centralized layout, pixel snap, correct aspect).
- No allocations in paint().

============================================================
IMPLEMENTER (STOP-gated)
============================================================
STEP 1 — Audit the scope render + scaling
- How is the arc drawn (StereoScopeView::paint / shared scope renderer)? How does it scale to bounds? Identify distortion or incorrect scale-on-resize, aliasing, missing grid/labels.
STOP and report current rendering + scaling approach and the visual gaps.

STEP 2 — Visual improvements
- Smooth/anti-aliased arc, correct amplitude scaling, optional persistence/fade, clear grid (L/R/Mono axes, anti-phase region), consistent theme colors (theme.seriesSide etc.). Keep both Basic and PAZ shapes working.
STOP and report visual changes.

STEP 3 — Resize/aspect
- Make the scope fill its panel and scale correctly when the panel resizes (maintain the arc's aspect; no horizontal/vertical stretch distortion). Integrate with the Stage-5 layout.
STOP and report the scaling/aspect fix.

STEP 4 — Build + eye-check
- Dev build, audio with stereo content. Confirm: clear arc, correct scaling, fills panel, resizes without distortion, Basic+PAZ both good.
STOP and write PROMPTS/MISSIONS/UI_OVERHAUL_04_RESULT.md. End with STOP.

============================================================
VERIFIER
============================================================
CHECK 1 — Render-only; no scope-DSP change.
CHECK 2 — Arc reads clearly (anti-aliased, correct scale, grid/labels), Basic+PAZ both work.
CHECK 3 — Fills + scales correctly on resize, no distortion; integrates with Stage-5 layout.
CHECK 4 — No paint() allocations.
OUTPUT: PROMPTS/MISSIONS/UI_OVERHAUL_04_VERIFIER.md. End with STOP.
