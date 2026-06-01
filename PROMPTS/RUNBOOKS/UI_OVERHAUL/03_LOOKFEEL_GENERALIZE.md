MISSION_ID: UI_OVERHAUL_03_LOOKFEEL_GENERALIZE

TITLE
Adopt the MasterLimiter look & feel in AnalyzerPro by generalizing its LookAndFeel overrides into the shared mdsp_ui::LookAndFeel — so all plugins inherit the polished buttons/sliders/toggles. REVIEW-GATED (owner reviews visually; architect final revision later).

GOAL
- AnalyzerPro buttons/combos/toggles/sliders match the MasterLimiter quality.
- The polish lives in shared mdsp_ui (not AnalyzerPro-local), so MultiBand/Spectrogram/etc. benefit too.

REFERENCE / FILES
- MasterLimiter/Source/ui/MasterLimiterLookAndFeel.{h,cpp} (522 lines) extends mdsp_ui::LookAndFeel and overrides: drawRotarySlider, drawLinearSlider, drawButtonBackground, drawButtonText, drawToggleButton.
- Shared base: third_party/.../mdsp_ui/{include,src} LookAndFeel.*, ButtonStyle.*, ButtonPaint.h, Theme.*, Typography.*, ThemeTokens.generated.h.
- AnalyzerPro currently has ad-hoc LnF in HeaderBar.cpp (drawButtonBackground/drawToggleButton) — to be replaced by the shared look.

HARD RULES
- Render-only (LookAndFeel/painting). No DSP, no behavior changes — visuals only.
- Generalize into mdsp_ui (shared) → commit in the submodule (pinned branch) + bump parent pointer.
- Keep theme-token driven (colors/typography from Theme/ThemeTokens) so per-plugin accents still work; do NOT hardcode MasterLimiter's accent colors into the shared base.
- This is SUBJECTIVE: implement the generalized look, then STOP for owner visual review before wiring it everywhere.

============================================================
IMPLEMENTER (STOP-gated)
============================================================
STEP 1 — Diff ML LnF vs the shared base
- Compare MasterLimiterLookAndFeel overrides against mdsp_ui::LookAndFeel. Identify which draw improvements are GENERIC (belong in the shared base) vs ML-accent-specific (must stay token-driven, not hardcoded).
STOP and report the per-method generalization plan (what moves to mdsp_ui, what stays token-driven).

STEP 2 — Generalize into mdsp_ui (shared)
- Move the generic improvements into mdsp_ui::LookAndFeel / ButtonStyle / ButtonPaint, parameterized by Theme tokens. MasterLimiter then keeps only truly ML-specific bits (ideally none → it can use the base).
- Build MasterLimiter too (sibling) to confirm no visual regression there.
STOP and report what moved + ML still builds/looks correct.

STEP 3 — Adopt in AnalyzerPro
- Replace AnalyzerPro's ad-hoc HeaderBar LnF with the shared mdsp_ui look; ensure all surfaces (header, rail, combos, toggles, footer) pick it up via the editor's LookAndFeel.
- Keep AnalyzerPro's accent/theme tokens.
STOP for OWNER VISUAL REVIEW. Write PROMPTS/MISSIONS/UI_OVERHAUL_03_RESULT.md with before/after notes; list anything subjective for architect final revision. End with STOP — do NOT proceed to other stages' restyling until owner approves.

============================================================
VERIFIER
============================================================
CHECK 1 — Render-only; no behavior/DSP change.
CHECK 2 — Polish lives in shared mdsp_ui (token-driven), not hardcoded per-plugin; AnalyzerPro accent preserved.
CHECK 3 — MasterLimiter still builds + looks correct (no shared-base regression).
CHECK 4 — Shared change committed in submodule + parent pointer bumped.
CHECK 5 — Owner visually approved before broader restyle.
OUTPUT: PROMPTS/MISSIONS/UI_OVERHAUL_03_VERIFIER.md. End with STOP.
