FILENAME: docs/SPEC.md
AnalyzerPro v1.0 — Product Specification (Freeze)
	1.	Scope (v1.0)
Included

	•	Views:
	•	RTA view
	•	FFT view
	•	Input formats:
	•	Stereo (2ch) and Mono (1ch)
	•	Channel focus (when 2ch):
	•	L, R, and L+R (Sum) selectable
	•	Markers:
	•	A/B markers + additional markers
	•	Delta readout between A and B
	•	Presets:
	•	Factory defaults + user presets
	•	Full UI + analyzer state recall
	•	Meters:
	•	Input meters on the left side
	•	Output meters on the right side

Explicitly excluded (v1.0)
	•	Spectrogram
	•	Correlation/phase view
	•	Multichannel (surround/immersive)
	•	External routing matrix, sidechain analysis
	•	Offline rendering/export

	2.	Primary Use Cases

	•	Live/FOH: fast spectral inspection, stable readouts, quick range/zoom, minimal UI clutter.
	•	Mixing/mastering: precise peak vs average, markers, deltas, preset recall, reliable state restore.
	•	Troubleshooting: identify resonances, hum fundamentals/harmonics, band energy over time (RTA).

	3.	Views and Semantics

3.1 RTA View (Real-Time Analyzer)
Definition
	•	RTA view displays band energy over frequency using octave-band smoothing selectable by the user.
	•	Visual is a stable “banded” response suitable for fast decisions.

RTA Controls (v1.0)
	•	Smoothing (octave fraction):
	•	Off, 1/3, 1/6, 1/12, 1/24, 1/48
	•	Default: 1/12
	•	Averaging / integration:
	•	Mode: None (instant), Slow, Medium, Fast (named presets mapped to time constants)
	•	Default: Medium
	•	Time constant mapping (ms), fixed in code for v1.0:
	•	Fast: 125 ms
	•	Medium: 300 ms
	•	Slow: 800 ms
	•	Peak hold:
	•	Off / On
	•	Default: On
	•	Peak decay:
	•	Slow / Medium / Fast
	•	Default: Medium
	•	Decay mapping, fixed in code for v1.0:
	•	Fast: 6 dB/s
	•	Medium: 3 dB/s
	•	Slow: 1.5 dB/s
	•	Freeze/Pause:
	•	Freezes analyzer updates, retains current curves and readouts.
	•	Default: Off
	•	Reset:
	•	Clears peak hold, clears averaging history, resets markers if configured to do so (see Section 7).

Axes and Scale
	•	Frequency axis:
	•	Log scale (fixed) for v1.0
	•	Frequency range displayed:
	•	20 Hz to 20 kHz (default)
	•	User adjustable min/max (see Section 6)
	•	Magnitude axis:
	•	dBFS display (0 dBFS at full-scale)
	•	Default display range: -90 dB to 0 dB
	•	User adjustable min/max (see Section 6)

3.2 FFT View
Definition
	•	FFT view displays spectrum magnitude derived from a windowed FFT.
	•	Intended for more detailed, higher-resolution inspection than RTA.

FFT Controls (v1.0)
	•	FFT size:
	•	1024, 2048, 4096, 8192, 16384, 32768
	•	Default: 8192
	•	Window type:
	•	Hann (fixed in v1.0)
	•	Overlap:
	•	75% (fixed in v1.0)
	•	Smoothing:
	•	Off, Light, Medium, Heavy (applies post-FFT magnitude smoothing)
	•	Default: Light
	•	Smoothing mapping, fixed in code for v1.0:
	•	Light: 1/24 octave equivalent
	•	Medium: 1/12 octave equivalent
	•	Heavy: 1/6 octave equivalent
	•	Peak hold:
	•	Off / On
	•	Default: On
	•	Peak decay:
	•	Slow / Medium / Fast
	•	Default: Medium (same mapping as RTA)
	•	Freeze/Pause:
	•	Same behavior as RTA (freezes updates)
	•	Reset:
	•	Clears peak hold and history.

Axes and Scale
	•	Frequency axis:
	•	Log scale (default)
	•	Optional: Linear scale (toggle)
	•	Default: Log
	•	Magnitude axis:
	•	dBFS
	•	Default display range: -120 dB to 0 dB
	•	User adjustable min/max (see Section 6)

	4.	Audio I/O Behavior

4.1 Input/Output signal path (Analyzer-only)
	•	Audio passes through unchanged (unity, no processing).
	•	Analyzer taps input and output for metering and analysis.
	•	Input meters reflect post-input, pre-processing (since no processing, effectively input).
	•	Output meters reflect post-processing output (unity path in v1.0).

4.2 Channel handling
When host provides mono (1ch)
	•	Channel focus selector is hidden or disabled.
	•	Analyzer uses the single channel as “Mono.”
	•	Input meter (left) is single meter.
	•	Output meter (right) is single meter.

When host provides stereo (2ch)
	•	Channel focus options:
	•	L
	•	R
	•	Sum (L+R) (normalized sum, not +6 dB)
	•	Default: Sum (L+R)
	•	Meters:
	•	Left side shows Input L and Input R meters (two meters).
	•	Right side shows Output L and Output R meters (two meters).

4.3 Sum normalization rule (v1.0)
	•	Sum = 0.5 * (L + R)
	•	This keeps the summed signal closer to comparable loudness and avoids systematic +6 dB gain.

	5.	Parameterization (Automation vs UI-only)

5.1 Automatable parameters (host-visible)
These are intended to be host-automated and appear in parameter list:
	•	View Mode: RTA / FFT
	•	Channel Focus (stereo only): L / R / Sum
	•	Freeze: Off / On
	•	RTA Smoothing: Off, 1/3, 1/6, 1/12, 1/24, 1/48
	•	RTA Averaging: None, Fast, Medium, Slow
	•	RTA Peak Hold: Off / On
	•	RTA Peak Decay: Fast, Medium, Slow
	•	FFT Size: 1024..32768
	•	FFT Scale: Log / Linear
	•	FFT Smoothing: Off, Light, Medium, Heavy
	•	FFT Peak Hold: Off / On
	•	FFT Peak Decay: Fast, Medium, Slow

Notes
	•	“Reset” is a momentary action and should not be a host parameter; implement as UI action with host state unaffected.
	•	Markers are session/UI state; not automatable in v1.0.

5.2 UI-only state (persisted in plugin state, not host-automatable)
	•	UI window size (if resizable)
	•	Meter visibility toggles (if added)
	•	Plot range (dB min/max)
	•	Frequency range (min/max)
	•	Zoom/pan viewport per view
	•	Marker list (if persistence enabled; see Section 7)
	•	Last cursor position (not persisted recommended)
	•	Theme preference (if added later)

	6.	Plot Interaction, Zoom, Range

6.1 Interactions (common to RTA and FFT)
	•	Cursor readout:
	•	Hover shows frequency + magnitude readout
	•	Readout updates at GUI frame rate, not audio rate
	•	Zoom:
	•	Vertical zoom: adjusts dB range
	•	Horizontal zoom: adjusts frequency span (min/max)
	•	Pan:
	•	Horizontal pan moves frequency range window
	•	Vertical pan moves dB range window
	•	Modifiers (fixed mapping for v1.0)
	•	Scroll wheel / trackpad:
	•	No modifier: horizontal zoom (frequency)
	•	Shift: vertical zoom (dB)
	•	Alt/Option: pan (both axes, depending on gesture)
	•	Double-click:
	•	Reset viewport to defaults for the current view

6.2 Default ranges
RTA defaults
	•	Freq: 20 Hz .. 20 kHz
	•	dB: -90 .. 0

FFT defaults
	•	Freq: 20 Hz .. 20 kHz
	•	dB: -120 .. 0

	7.	Markers (v1.0)

7.1 Marker types
	•	Marker A (primary)
	•	Marker B (secondary)
	•	Additional markers (numbered) (optional but included in v1.0 scope)

7.2 Marker operations
	•	Add marker at cursor
	•	Drag marker along frequency axis
	•	Remove marker
	•	Snap (optional v1.0): Off by default; if enabled, snap to nearest FFT bin in FFT view only

7.3 Delta readout
	•	Shows:
	•	ΔFreq (Hz)
	•	ΔdB (dB)
	•	Delta is computed between marker A and marker B.
	•	If either marker missing, delta panel shows “—”.

7.4 Persistence (v1.0 decision)
	•	Markers persist in plugin state by default (so sessions reopen correctly).
	•	Presets include marker state by default.

	8.	Meters (v1.0)

8.1 Meter placement
	•	Input meters are on the left side of the plot
	•	Output meters are on the right side of the plot

8.2 Meter type and ballistics
	•	Level type:
	•	Peak + RMS overlay (preferred)
	•	If constrained: peak only (minimum acceptable)
	•	Ballistics (fixed in v1.0):
	•	Peak: 10 ms attack, 300 ms release
	•	RMS: 50 ms window, 300 ms release (if RMS implemented)
	•	Scale:
	•	dBFS
	•	Meter range: -60 dB to 0 dB (configurable later; fixed in v1.0)
	•	Clipping:
	•	Clip indicator lights when sample >= 0 dBFS
	•	Clip latch until reset (Reset button clears it)

8.3 Channel layout
Mono
	•	Left: In (Mono)
	•	Right: Out (Mono)

Stereo
	•	Left: In L, In R
	•	Right: Out L, Out R

	9.	Presets and State

9.1 Preset contents
Presets must store:
	•	View mode
	•	Channel focus
	•	All RTA controls
	•	All FFT controls
	•	Plot ranges (freq + dB) per view
	•	Marker state
	•	Meter clip latch state (optional; recommended: do not store latch, reset on load)

9.2 State versioning
	•	Plugin state must include:
	•	schemaVersion integer
	•	stable keys for each setting
	•	On load:
	•	if schemaVersion older, migrate to current defaults

	10.	Performance Requirements

	•	No heap allocation on audio thread.
	•	Analyzer worker uses bounded, preallocated buffers.
	•	Paint must not allocate; render from snapshot only.
	•	GUI updates:
	•	Plot refresh target: 30–60 Hz
	•	Snapshot publishing: decoupled from GUI, limited by worker.

	11.	QA Acceptance Criteria (v1.0)
Functional

	•	Stereo and mono hosts both behave correctly.
	•	RTA and FFT modes switch without glitches or stuck state.
	•	Freeze stops updates, unfreeze resumes.
	•	Peak hold and decay behave per mapping.
	•	Markers add/drag/remove; delta readout accurate.
	•	Presets recall all state and match what is visible.

Stability
	•	No crashes on rapid resize, mode switching, or preset switching.
	•	No denormals or CPU spikes when silent input.
	•	No audio dropouts attributable to analyzer.

End of SPEC.md

FILENAME: docs/UI_WIREFRAME.md
AnalyzerPro v1.0 — UI Wireframe and Interaction Map
	1.	Layout Overview
Primary goal: maximize plot area while providing fast, minimal-control access and clear metering.

Top-level layout (stereo example)

+———————————————————————————–+
| [Mode: RTA | FFT]   [Channel: L | R | Sum]   [Freeze]   [Reset]   [Preset: ____ v]|
+———————————————————————————–+
|  IN METERS (L/R)   |                                               | OUT METERS  |
|  [In L]            |                 PLOT CANVAS                    |  [Out L]    |
|  [In R]            |         (RTA or FFT renderer output)           |  [Out R]    |
|                    |                                                |  [Out L pk] |
|                    |                                                |  [Out R pk] |
+———————————————————————————–+
| Context Strip (changes with mode)                                                   |
| RTA: [Smooth v] [Avg v] [PeakHold] [Decay v] [Range -90..0] [Freq 20..20k]         |
| FFT: [Size v] [Scale Log/Linear] [Smooth v] [PeakHold] [Decay v] [Range -120..0]   |
+———————————————————————————–+
| Marker/Readout Bar: [Cursor: 997 Hz  -18.2 dB]  [A: 1.0k/-20.1] [B: 2.0k/-24.0]    |
|                     [Δ: 1000 Hz   -3.9 dB]                                         |
+———————————————————————————–+

Mono variant
	•	Channel selector hidden/disabled
	•	Left meter shows In (Mono)
	•	Right meter shows Out (Mono)

	2.	Component Tree (recommended)
PluginEditor

	•	TopBar
	•	ModeSegmentedControl (RTA/FFT)
	•	ChannelSegmentedControl (L/R/Sum) [visible only when stereo]
	•	FreezeToggle
	•	ResetButton (momentary)
	•	PresetControl (name + dropdown + save)
	•	MainRow
	•	InputMetersPanel (left)
	•	MeterInL (or MeterInMono)
	•	MeterInR (stereo only)
	•	ClipIndicators + Reset Clip (optional)
	•	PlotView (center)
	•	PlotViewportState (freqMin/freqMax, dBMin/dBMax)
	•	CursorState
	•	MarkerState (A/B/extra)
	•	Delegates painting to IPlotRenderer
	•	OutputMetersPanel (right)
	•	MeterOutL (or MeterOutMono)
	•	MeterOutR (stereo only)
	•	ClipIndicators + Reset Clip (optional)
	•	ContextStrip (bottom)
	•	RTAControlsPanel OR FFTControlsPanel (swap on mode)
	•	ReadoutBar (bottom-most)
	•	CursorReadout
	•	MarkerAReadout
	•	MarkerBReadout
	•	DeltaReadout

	3.	Control Definitions

3.1 TopBar controls
Mode
	•	RTA / FFT segmented control
	•	Immediate switch, no modal.

Channel focus (stereo only)
	•	L / R / Sum segmented control
	•	Affects plot and readouts only; meters remain per-channel.

Freeze
	•	Toggle
	•	Freezes analyzer updates (plot + readout). Meters continue updating unless explicitly frozen (v1.0: meters continue).

Reset (momentary)
	•	Clears:
	•	Peak holds (current mode)
	•	Averaging history (RTA)
	•	Marker state (optional: configurable; v1.0 default: keep markers)
	•	Meter clip latches (if enabled)

Presets
	•	Dropdown list of presets
	•	Save/Overwrite button(s)
	•	Optional: A/B preset compare (not in v1.0)

3.2 Context strip controls (mode-specific)
RTA strip
	•	Smoothing dropdown: Off, 1/3, 1/6, 1/12, 1/24, 1/48
	•	Averaging dropdown: None, Fast, Medium, Slow
	•	Peak Hold toggle
	•	Decay dropdown: Fast, Medium, Slow
	•	Range control:
	•	Two numeric fields or a dual-slider (dB min/max)
	•	v1.0 minimum: dropdown presets (-90..0, -72..0, -60..0)
	•	Freq range control:
	•	Two numeric fields (Hz) with validation and clamping
	•	v1.0 minimum: presets (20..20k, 40..20k, 20..10k)

FFT strip
	•	FFT size dropdown: 1024..32768
	•	Scale toggle: Log / Linear
	•	Smoothing dropdown: Off, Light, Medium, Heavy
	•	Peak Hold toggle
	•	Decay dropdown: Fast, Medium, Slow
	•	Range control:
	•	presets (-120..0, -96..0, -72..0)

	4.	Plot Interaction Map

4.1 Cursor behavior
	•	Hover over plot shows:
	•	Frequency (Hz)
	•	Magnitude (dB)
	•	Cursor readout always visible in ReadoutBar.
	•	Cursor is hidden when outside plot bounds.

4.2 Zoom and pan (v1.0 fixed mapping)
Scroll wheel / trackpad
	•	Default: horizontal zoom (frequency)
	•	Shift + scroll: vertical zoom (dB)
	•	Alt/Option + scroll: pan
	•	trackpad: pan follows gesture
	•	mouse wheel: vertical pan (dB) unless Shift is also held (then horizontal pan)

Double click
	•	Reset viewport to mode defaults.

Drag
	•	Left drag on empty plot area:
	•	pans horizontally (frequency)
	•	Shift + drag:
	•	pans vertically (dB)
	•	Dragging a marker:
	•	moves marker frequency (and updates its magnitude readout)

4.3 Marker interactions
Add marker
	•	Key command: M adds a numbered marker at cursor
	•	A sets Marker A at cursor
	•	B sets Marker B at cursor
Alternative (if no keyboard focus):
	•	Right-click context menu:
	•	Add Marker
	•	Set A
	•	Set B
	•	Clear Markers

Remove marker
	•	Select marker and press Delete/Backspace
	•	Context menu “Remove marker”

Marker snap (FFT only; v1.0 default Off)
	•	If enabled, marker frequency snaps to nearest FFT bin.

	5.	Meter UI Details

5.1 Meter presentation
	•	Vertical meters
	•	Labeling:
	•	“IN” at top of left panel, “OUT” at top of right panel
	•	Each meter labeled L/R or Mono
	•	Clip indicator:
	•	small LED at top; latch until reset

5.2 Update behavior
	•	Meters update even when Freeze is enabled (v1.0).
	•	Optional later: “Freeze meters” setting (not in v1.0).

	6.	Resizing and DPI

	•	v1.0 recommended:
	•	Resizable window with min size constraints
	•	UI scales proportionally
	•	Minimum sizes
	•	Ensure plot remains usable (min width/height thresholds)
	•	Text legibility on high DPI is non-negotiable.

	7.	Accessibility / Focus

	•	All top bar controls keyboard navigable (tab)
	•	Plot view accepts focus for key commands (A/B/M/Delete)
	•	Visible focus ring on the focused control (small but clear)

End of UI_WIREFRAME.md

FILENAME: docs/IMPLEMENTATION_TASKS.md
AnalyzerPro v1.0 — Implementation Task List (Dependency-Ordered)

Conventions
	•	“Done” means: compiles in Debug/Release, no warnings, and functionally verified in at least one AU/VST3 host.
	•	No heap allocations in audio callback.
	•	No allocations in paint() paths.

T01 — Freeze the v1.0 specification into code constants
Goal
	•	Create a single source of truth for defaults and enumerations.

Work
	•	Add enums for:
	•	ViewMode { RTA, FFT }
	•	ChannelFocus { Mono, L, R, Sum } (Mono used when host mono)
	•	RtaSmoothing { Off, O3, O6, O12, O24, O48 }
	•	RtaAveraging { None, Fast, Medium, Slow }
	•	DecaySpeed { Fast, Medium, Slow }
	•	FftSize { 1024..32768 }
	•	FftScale { Log, Linear }
	•	FftSmoothing { Off, Light, Medium, Heavy }
	•	Add default constants per SPEC.md
	•	Add mapping functions for time constants/decay rates

Done criteria
	•	Defaults match docs/SPEC.md exactly.
	•	One header contains all canonical enums/defaults.

T02 — Define AnalyzerSnapshot (immutable render payload)
Goal
	•	GUI paints from snapshots only (no thread coupling).

Work
	•	Create struct AnalyzerSnapshot with:
	•	sampleRate, fftSize, channelCount
	•	viewMode
	•	frequency axis metadata (min/max)
	•	magnitude range metadata (dB min/max)
	•	data buffers:
	•	rtaBands[] magnitude dB
	•	fftBins[] magnitude dB
	•	marker readout helpers (optional)
	•	timestamp / sequence number
	•	Ensure snapshot owns its buffers (no external references).

Done criteria
	•	Snapshot is trivially safe to pass across threads (copy or shared ownership).
	•	No GUI reads from analyzer internals directly.

T03 — Build lock-free publishing of snapshots (worker → GUI)
Goal
	•	Analyzer worker produces snapshots; GUI consumes latest safely.

Work
	•	Implement one of:
	•	atomic shared_ptr swap, or
	•	lock-free pointer swap to preallocated snapshot slots (triple buffer)
	•	Add “latestSnapshotSequence” to skip redundant paints.

Done criteria
	•	No locks on audio thread.
	•	GUI always paints the most recent complete snapshot.

T04 — Implement audio ring buffer capture (audio thread)
Goal
	•	Capture input and output streams for analysis and meters.

Work
	•	Implement lock-free ring buffer(s) sized for max FFT window + overlap.
	•	Tap both input and output buffers:
	•	In capture: from incoming audio
	•	Out capture: from processed output (unity in v1.0)
	•	Handle mono and stereo cleanly.

Done criteria
	•	Works in mono and stereo hosts.
	•	No allocations on audio callback.
	•	No overruns under normal CPU load.

T05 — Implement meter engine (input + output)
Goal
	•	Provide stable peak (and optionally RMS) values with clip latch.

Work
	•	Add MeterState per channel:
	•	peak, rms (optional), clipLatched
	•	Update in audio callback using envelope ballistics:
	•	peak attack/release
	•	rms windowing (if included)
	•	Expose meter values to GUI via atomic floats or snapshot fields.

Done criteria
	•	Stereo: In L/In R on left, Out L/Out R on right.
	•	Mono: In Mono / Out Mono.
	•	Clip latch resets via Reset action.

T06 — Implement FFT engine (worker thread)
Goal
	•	Windowed FFT magnitude and post-smoothing.

Work
	•	Preallocate FFT buffers for max size.
	•	Window: Hann
	•	Overlap: 75% (hop size = N/4)
	•	Magnitude to dBFS conversion with floor clamp (avoid -inf)
	•	Implement FFT smoothing levels mapping from SPEC.md.

Done criteria
	•	Correct output across all FFT sizes.
	•	Stable CPU; no allocations in worker loop.

T07 — Implement RTA engine (worker thread)
Goal
	•	Octave-band smoothing and averaging.

Work
	•	Choose implementation:
	•	derive from FFT magnitude and apply band aggregation, or
	•	direct filterbank (not recommended for v1.0 unless already present)
	•	Implement smoothing fractions per SPEC.md.
	•	Implement averaging time constants (Fast/Medium/Slow) as EMA or IIR in worker.
	•	Implement peak hold + decay.

Done criteria
	•	Visual matches expected “RTA” behavior.
	•	Averaging controls audibly/visually do what the names imply.

T08 — Implement PlotView (single plot component)
Goal
	•	One component that handles viewport, cursor, markers, gestures, and delegates paint.

Work
	•	PlotView owns:
	•	viewport state (freq min/max, dB min/max)
	•	cursor state (freq, dB)
	•	marker state (A, B, list)
	•	gesture logic (zoom/pan)
	•	PlotView pulls latest AnalyzerSnapshot on a timer.

Done criteria
	•	No per-mode plot components.
	•	Cursor and markers work in both views.

T09 — Implement renderers: RTAPlotRenderer + FFTPlotRenderer
Goal
	•	Paint logic isolated per view, using snapshot + viewport.

Work
	•	Define interface IPlotRenderer:
	•	paint(Graphics&, const AnalyzerSnapshot&, const ViewportState&, const MarkerState&, const CursorState)
	•	Implement:
	•	RTAPlotRenderer: banded curve + peak hold
	•	FFTPlotRenderer: line curve + peak hold
	•	No allocations in paint.
	•	Cache expensive transforms (freq→x) safely in PlotView if needed.

Done criteria
	•	Both renderers produce correct visuals and respect viewport ranges.

T10 — Implement TopBar controls and ContextStrip panels
Goal
	•	Wire UI controls to parameters/state and keep UI consistent.

Work
	•	TopBar:
	•	mode switch, channel focus, freeze, reset, preset control
	•	ContextStrip:
	•	swap panel based on mode
	•	RTA panel controls (smoothing, averaging, peak hold, decay, ranges)
	•	FFT panel controls (fft size, scale, smoothing, peak hold, decay, ranges)

Done criteria
	•	Changing a control updates analyzer + plot deterministically.
	•	No UI hangs on rapid toggling.

T11 — Implement parameter model (host automation)
Goal
	•	Host-visible parameter list per SPEC.md.

Work
	•	Create parameters for:
	•	view mode, channel focus, freeze
	•	RTA set
	•	FFT set
	•	Ensure mono hosts ignore channel focus gracefully.
	•	Ensure UI reflects parameter changes from host automation.

Done criteria
	•	Parameters appear correctly in host and automate.
	•	No desync between UI and DSP state.

T12 — Implement markers persistence and preset integration
Goal
	•	Markers included in state and presets by default.

Work
	•	State serialization includes:
	•	marker A/B, additional markers
	•	viewport ranges per mode
	•	relevant UI-only state per SPEC.md
	•	Preset save/load covers all required fields.
	•	Decide reset behavior:
	•	v1.0 default: Reset clears peak/avg, does not clear markers
	•	Add “Clear Markers” explicit action in marker menu

Done criteria
	•	Session reload restores markers and viewports.
	•	Presets recall markers reliably.

T13 — Implement mono/stereo UI switching
Goal
	•	UI adapts to channel count.

Work
	•	Detect channel count from processor.
	•	Hide/disable channel selector in mono.
	•	Meters panel switches between mono and stereo layouts.

Done criteria
	•	Mono hosts show correct meters and analysis.
	•	No layout glitches when host changes configuration (if supported).

T14 — Implement performance guardrails
Goal
	•	Prevent expensive work when not needed.

Work
	•	Early-outs:
	•	If sampleRate invalid, show “NO DATA” overlay in plot
	•	If fftSize invalid, show overlay
	•	Limit GUI refresh rate to 30–60 Hz.
	•	Ensure worker does bounded work per cycle.

Done criteria
	•	No spikes in CPU on silence or idle.
	•	Plot overlay messages are clear and correct.

T15 — Implement QA test checklist (manual)
Goal
	•	A repeatable manual test pass for each build.

Work
	•	Create docs/QA_CHECKLIST.md (short)
	•	Include:
	•	mono host test
	•	stereo test
	•	preset save/load
	•	automation of key params
	•	resize stress test
	•	rapid mode switching
	•	freeze/reset behavior

Done criteria
	•	Checklist exists and is used at least once to validate a build.

End of IMPLEMENTATION_TASKS.md