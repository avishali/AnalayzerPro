# UI_OVERHAUL_02_CONTROL_IA_REORG — RESULT

Date: 2026-06-01

## Binding Parity Gate
- ControlRail was verified as a full editable superset before detaching the popup flow.
- Rail bindings retained: FFT Size, Detail, Tilt, Smoothing, Weighting, Hold, Release, Scope Input, Scope Hold, Meter Input, Meter Hold, and all seven trace toggles.
- Trace bindings include `TraceShowSide` -> `analyzerShowSide`; Show Side is now in the Traces rail module.
- Popup-only callback controls already had rail equivalents: analyzer mode buttons and scope mode/shape callbacks.

## IA As Built
- Header module buttons now act as settings tabs for the side rail: Spectrum, Scopes, Meters, Traces.
- Clicking a module opens the rail if closed and selects that module; clicking the already-active open module closes the rail.
- Header active state is shown only while the rail is open, and it stays in sync with the rail show/hide button.
- ControlRail now uses active-module filtering instead of the previous CollapsibleSection model.

## Module Groups
- Spectrum: FFT/BAND/LOG mode, FFT Size, Detail, Smoothing, Weighting, Tilt, Hold, Reset, Release Time.
- Scopes: Scope Mode, Scope Shape, Scope Input, Scope Hold.
- Meters: Meter Input, Meter Hold.
- Traces: Show Stereo, Show Mono, Show Left, Show Right, Show Mid, Show Side, Show RMS.

## Duplication Resolution
- SettingsPopupPanel remains in the build but is detached from MainView.
- No CMake/build metadata was changed.
- The live editable popup path is removed; the rail is the single active editable control surface for the module controls.
- Footer global Hold/Release remains unchanged per the master IA.

## Unmapped Rail Content
- Navigate placeholder was dropped.
- Display section was folded into Spectrum through Tilt.
- Meter placeholder was dropped until there is real loudness/meter config to expose.

## Build
- Dev build passed:
  `cmake --build build-stage1-dev --config Debug -j 8`
- Existing warning noise remains in unrelated/shared code; no compile errors.

## Visual Review Items
- Owner should eye-check module tab switching, rail show/hide sync, and overflow scrolling.
- Owner should confirm the subjective Spectrum ordering, especially whether Release Time should remain in Spectrum now that Footer also exposes it.
- Owner should confirm Traces visibility and that Show Side is reachable without scrolling surprises at minimum rail height.

STOP
