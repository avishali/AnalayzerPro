MISSION_ID: UI_OVERHAUL_06_METERS_FINISH

TITLE
Final UI pass on the meters (L/R in/out level meters + LUFS momentary/short/integrated/peak), reusing the MasterLimiter meter work where it's shared.

GOAL
- Meters look finished and consistent with the rest of the UI: clean scale/ticks, correct ballistics, peak-hold marker, clip indicator, readable numeric LUFS panel.
- Reuse MasterLimiter's meter components/ballistics rather than reinventing — generalize shared pieces into mdsp_ui where it makes sense (per the shared-design decision).

REFERENCE / FILES
- AnalyzerPro: Source/ui/meters/*, Source/ui/loudness/*.
- MasterLimiter (sibling): Source/ui/meters/MeterGroupComponent.h, ClipBallistics.cpp, Source/ui/loudness/LoudnessNumericPanel.cpp — these are the polished versions to mirror/extract.
- Shared: third_party/.../mdsp_ui (BarsRenderer, MarkerRenderer, ScaleLabelRenderer, meters/).

HARD RULES
- Render-only; meter DSP/ballistics values come from the engine — don't change metering math (you may reuse ML's ballistics RENDER smoothing, but not alter measured values).
- Shared components extracted to mdsp_ui must be committed in the submodule (pinned branch) + parent pointer bumped.
- No paint() allocations.

============================================================
IMPLEMENTER (STOP-gated)
============================================================
STEP 1 — Compare AnalyzerPro meters vs MasterLimiter
- List the differences: scale/ticks, ballistics, peak-hold, clip indicator, numeric LUFS panel styling. Identify which MasterLimiter pieces are already in mdsp_ui vs ML-local (MeterGroupComponent, ClipBallistics, LoudnessNumericPanel).
STOP and report the gap list + what is shared vs ML-local.

STEP 2 — Generalize/adopt
- For ML-local meter pieces worth sharing, extract them into mdsp_ui (shared) and have BOTH plugins use them; otherwise mirror the styling in AnalyzerPro's meter components. Apply: consistent scale/ticks, ballistics, peak-hold marker, clip indicator, LUFS numeric panel.
STOP and report what was extracted to mdsp_ui vs applied locally.

STEP 3 — Build + eye-check
- Dev build, audio. Confirm meters: correct scale, smooth ballistics, peak-hold + clip behave, LUFS panel readable and consistent with the theme; in/out meters match.
STOP and write PROMPTS/MISSIONS/UI_OVERHAUL_06_RESULT.md. End with STOP.

============================================================
VERIFIER
============================================================
CHECK 1 — Render-only; measured meter/LUFS values unchanged.
CHECK 2 — Visual parity with MasterLimiter polish (scale, ballistics, peak-hold, clip, LUFS panel).
CHECK 3 — Shared extractions committed in submodule + parent pointer bumped.
CHECK 4 — No paint() allocations.
OUTPUT: PROMPTS/MISSIONS/UI_OVERHAUL_06_VERIFIER.md. End with STOP.
