MISSION_ID: UI_OVERHAUL_05_PANEL_RESIZE_LOCK

TITLE
Lock panel placement and make resizing stable — no jumping/overlap/clipping, deterministic layout, sane min/max window size. Foundational for the phase-scope stage.

GOAL
- One authoritative layout pass; every panel (spectrum, phase scope, meters, loudness, rail, header, footer) lands in a deterministic rectangle on every resize.
- Window has resize limits + (if appropriate) aspect/min size so layout never collapses or overlaps.
- No first-frame jump, no panels drifting or overlapping during/after resize.
- Window size persists across open/close where the host allows.

LIKELY FILES
- Source/ui/MainView.cpp/.h (top-level resized() layout), Source/ui/layout/LayoutConstants.h, PixelSnap.h, ControlRail/HeaderBar/FooterBar, the panel components (RTADisplay host, scope, meters, loudness). PluginEditor for setResizeLimits/constrainer + persisted size.

HARD RULES
- Render/layout only — no DSP/engine/snapshot changes.
- Layout math centralized (LayoutConstants), pixel-snapped (PixelSnap) to avoid sub-pixel drift.
- No allocations in resized()/paint() hot paths.

============================================================
IMPLEMENTER (STOP-gated)
============================================================
STEP 1 — Audit current layout
- Map who positions what: MainView::resized() and each child's resized(). Identify overlaps, hardcoded magic numbers, and any component that sizes itself off stale/zero bounds (first-frame jump).
STOP and report the layout map + the specific instability causes found.

STEP 2 — Centralize + pixel-snap
- Route all region math through LayoutConstants + snapRectToPixels. One resized() per component, deriving children from a single computed layout (header/rail/footer/content split, then content → spectrum/scope/meters).
- Remove duplicated/competing layout math.
STOP and report the centralized layout structure.

STEP 3 — Resize limits + stability
- In the editor: setResizeLimits(minW,minH,maxW,maxH) (and a constrainer/aspect if the design needs it) so panels never collapse below usable size.
- Guard against zero/!isVisible bounds; ensure deterministic placement on the first resized() (no jump).
STOP and report the limits chosen + how first-frame jump is prevented.

STEP 4 — Persist window size
- Save/restore the editor size (APVTS state or editor size hint) so reopening keeps the user's size, within limits.
STOP and report persistence mechanism.

STEP 5 — Build + eye-check
- Dev build. Resize slowly and rapidly, drag corners, min/max, reopen the editor:
  - No overlap, no clipping, no drift, no first-frame jump; panels stable and proportional; size persists.
STOP and write PROMPTS/MISSIONS/UI_OVERHAUL_05_RESULT.md. End with STOP.

============================================================
VERIFIER
============================================================
CHECK 1 — Single authoritative, pixel-snapped layout; no competing math.
CHECK 2 — Resize limits set; layout never collapses/overlaps at min or during drag.
CHECK 3 — No first-frame jump; deterministic placement.
CHECK 4 — Size persists across reopen.
CHECK 5 — No DSP changes; no resized()/paint() allocations.
OUTPUT: PROMPTS/MISSIONS/UI_OVERHAUL_05_VERIFIER.md table. End with STOP.
