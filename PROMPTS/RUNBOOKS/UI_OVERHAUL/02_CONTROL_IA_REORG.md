MISSION_ID: UI_OVERHAUL_02_CONTROL_IA_REORG

TITLE
Reorganize the user controls into one clear, module-context information architecture: eliminate the ControlRail/SettingsPopupPanel duplication, group controls by the active module, and make every control (incl. Show Side) reliably visible. REVIEW-GATED — implement the proposed IA, owner reviews visually, architect final revision later.

PROPOSED IA (from 00_MASTER_PLAN.md — the spec to build; owner adjusts after seeing it)
- HEADER (global): Module tabs [Spectrum | Scopes | Meters | Traces] · Bypass · A/B · Preset · Save · global view (dB range, freq zoom/reset).
- SIDE RAIL (context-sensitive to the active module tab; collapsible) shows ONLY the active module's controls:
    Spectrum: FFT Size · Detail · Smoothing · Weighting · Tilt · Freq Range/Zoom · Hold/Reset
    Scopes:   Scope Mode · Shape · Input · Hold
    Meters:   Meter Input · Hold · (loudness/meter config)
    Traces:   Show Stereo / Mono / Left / Right / Mid / Side / RMS · Peak · Hold
- FOOTER (global): status · Hold · Release.
- SINGLE SOURCE OF TRUTH: the RAIL owns the controls. SettingsPopupPanel must NOT duplicate editable controls — remove it, or repurpose as a read-only overview. (Owner confirms in STEP 2.)

WHY
Today ControlRail AND SettingsPopupPanel carry near-identical controls (FFT Size, Tilt, Smoothing, Weighting, Hold, Scope Mode/Shape/Input/Hold, Meter Input/Hold, Show L/R/Mid/Side/RMS/Mono/Stereo, FFT/BAND/LOG) — two sources of truth, flat grouping, and unreliable surfacing of some toggles (Show Side). This is the core "messy / where-is-what" problem.

LIKELY FILES
- Source/ui/layout/ControlRail.{cpp,h}, SettingsPopupPanel.{cpp,h}, HeaderBar.{cpp,h}, MainView.{cpp,h}, ControlPanel.{cpp,h}, LayoutConstants.h.
- Param binding: ControlId::* mapping (binder_->bindCombo/bindToggle); keep all APVTS attachments intact when moving controls.

HARD RULES
- Render/UI-wiring only — no DSP/param-ID changes (controls keep their existing ControlId bindings; you are MOVING widgets, not re-defining params).
- Every control must remain functional and bound after the move (no orphaned/duplicate APVTS attachments).
- Do Stage 5 (resize stability) first so the new rail layout is stable.
- SUBJECTIVE: implement the proposed IA, then STOP for owner review. Do not invent new behaviors beyond the IA without flagging.

============================================================
IMPLEMENTER (STOP-gated)
============================================================
STEP 1 — Confirm the live inventory + bindings
- Produce the definitive list: every control, which ControlId it binds, and where it currently lives (rail vs popup vs header vs footer). Note the duplicates (same ControlId bound in two places).
STOP and report the inventory+binding table.

STEP 2 — Decide the duplication resolution (owner)
- Recommend: rail = single source of truth; remove SettingsPopupPanel's editable controls (or make it a read-only overview). Confirm with owner before deleting/rewiring.
STOP for owner decision on the popup's fate.

STEP 3 — Make the rail context-sensitive
- Drive the rail's visible control group from the active HeaderBar module tab (Spectrum/Scopes/Meters/Traces). Only the active module's controls show; switching tabs swaps the rail content. Keep collapse/expand.
- Ensure each control keeps its existing ControlId binding when relocated; no duplicate attachments.
STOP and report the tab→rail-group wiring.

STEP 4 — Group + surface all controls (incl. Show Side)
- Lay out each module's controls per the IA, logically grouped with clear labels/tooltips. Ensure the Traces group shows ALL of Stereo/Mono/L/R/Mid/Side/RMS reliably (fixes the missing-Side surfacing). Coordinate with the spawned Side-trace task — this stage may subsume it.
STOP and report the per-module grouping + Side now visible.

STEP 5 — Remove the duplication
- Per STEP 2 decision, remove/neuter SettingsPopupPanel's duplicate editable controls so there's one source of truth. Verify no control lost its binding and nothing is now unreachable.
STOP and report what was removed/repurposed.

STEP 6 — Build + OWNER VISUAL REVIEW
- Dev build. Walk each module tab; confirm controls are grouped, clear, all reachable, all functional (changing each affects the plugin), Side visible, no duplicates.
STOP and write PROMPTS/MISSIONS/UI_OVERHAUL_02_RESULT.md with the new IA as built + a list of subjective choices for architect final revision. End with STOP.

============================================================
VERIFIER
============================================================
CHECK 1 — UI-wiring only; all controls keep their original ControlId bindings; no duplicate/orphaned APVTS attachments.
CHECK 2 — Rail is context-sensitive to the module tab; only the active module's controls show.
CHECK 3 — Single source of truth (no editable duplication between rail and popup).
CHECK 4 — Every control reachable + functional, incl. Show Side; logical grouping + labels/tooltips.
CHECK 5 — Layout stable (built on Stage 5); no overlap/clipping.
OUTPUT: PROMPTS/MISSIONS/UI_OVERHAUL_02_VERIFIER.md. End with STOP.
