

Return only the final content changes in the repo (multiple files OK). No markdown. No explanations. JUCE C++17. Keep style consistent with AnalyzerPro.

Goal: Replace the “Release Time” control in AnalyzerPro sidebar:
	•	Remove the +/- buttons entirely.
	•	Make the value itself interactive:
	1.	On hover: mouse cursor becomes a vertical-drag/scroll style cursor (use the closest JUCE cursor type, e.g. UpDownResizeCursor).
	2.	On drag up/down: changes Release Time continuously.
	3.	Also support mouse wheel on hover to change value (optional but preferred).
	4.	Keep the existing parameter + range + text formatting (ms, s if you already do that) and keep automation intact.

Behavior requirements:
	•	Dragging up increases release time, dragging down decreases.
	•	Use pixel-to-value mapping that feels “plugin standard”:
	•	Default sensitivity: ~100 px for full range OR a sensible step mapping if range is huge.
	•	Fine adjust when Shift is held (e.g. 1/10 sensitivity).
	•	Coarse adjust when Cmd/Ctrl is held (e.g. 2x sensitivity). If Cmd is already used, pick Alt for coarse.
	•	Clamp to parameter range.
	•	Don’t create new parameters. Must write through the existing parameter (APVTS/parameter pointer/attachment).
	•	On mouseDown: beginChangeGesture()
	•	On mouseUp: endChangeGesture()
	•	During drag: setValueNotifyingHost() (or APVTS setValue + notify) so host automation writes correctly.
	•	Don’t break keyboard focus / tabbing; keep it accessible.

Implementation approach (use whichever matches current codebase best):
A) If Release Time is currently a Slider:
	•	Replace the Slider’s IncDecButtons with a custom “DraggableValueLabel” bound to the same parameter.
	•	Or switch Slider style to a drag-based style but keep only value text visible (no track/thumb).

B) If Release Time is currently a custom component with +/-:
	•	Delete the +/- buttons and implement mouse handlers on the value label component.

UI:
	•	Preserve existing layout spacing.
	•	The value label should visually indicate hover (subtle highlight or underline) if you already have theme tokens. No heavy redesign.

Files:
	•	Touch the minimal set of UI files that define this control (likely the Analyzer sidebar panel/component).
	•	Remove unused button members, listeners, and layout code cleanly.

Deliverables:
	•	Code compiles (Debug/Release).
	•	No warnings introduced.
	•	Release time changes are smooth and do not jump on drag start:
	•	Capture start value on mouseDown.
	•	Use total drag delta (not per-event delta) to compute new value.
	•	Cursor changes only when hovering the value label (not the entire panel).

Also include:
	•	A small internal helper component if needed, e.g. DraggableParamValueLabel:
	•	Knows param range
	•	Formats text
	•	Handles hover cursor
	•	Handles drag/wheel
	•	Uses parameter gesture calls

Definition of Done:
	•	+/- buttons are gone.
	•	Hover cursor changes on the value.
	•	Vertical drag adjusts release time and records automation.
	•	No regressions in other controls.

VERIFIER PROMPT (copy/paste into your IDE agent)

Return only a checklist of results + any failing items. No markdown.

Verify Release Time drag-value control:
	1.	UI/UX

	•	Hover on Release Time value changes cursor to vertical-drag/scroll-like cursor.
	•	+/- buttons are removed from UI and no dead space remains.
	•	Hover state is subtle and consistent with theme (if implemented).

	2.	Drag behavior

	•	MouseDown stores initial value; drag does not jump.
	•	Drag up increases, drag down decreases.
	•	Clamping works at min/max.
	•	Shift = fine adjustment; Cmd/Ctrl(or Alt) = coarse adjustment.
	•	MouseUp ends gesture; no “stuck gesture”.

	3.	Host / automation correctness

	•	beginChangeGesture() called on mouseDown.
	•	Value changes during drag use notifying host (automation write).
	•	endChangeGesture() called on mouseUp.
	•	Test in a host:
	•	Write automation by dragging value.
	•	Playback automation and confirm parameter follows.

	4.	Mouse wheel (if implemented)

	•	Wheel over value changes release time.
	•	Wheel changes also notify host (automation writes when armed if applicable).

	5.	Regression checks

	•	No leaks/crashes when opening/closing plugin window.
	•	Sidebar layout unchanged except removal of +/-.
	•	No new warnings.
	•	Builds Debug + Release.

If any item fails, list exact file/line and suspected cause.