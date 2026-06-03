# AnalyzerPro — Metal Renderer: Cursor Prompts (Phases B0–B5)

Decision locked: **Metal-native** for all phases (no OpenGL fallback in the build path; CPU
renderer remains the only non-GPU fallback). Companion design doc: `docs/metal_renderer_plan.md`.

**How to use:** For each phase, paste **§0 CONTEXT** (below) **plus** the phase's prompt into
Cursor. Run one phase, then **STOP** at its checkpoint — sign, install, test in Pro Tools, and
confirm before moving on. Each phase leaves the plugin shippable (flag off ⇒ current CPU UI).

---

## §0 CONTEXT (prepend to every phase prompt)

You are working in the AnalyzerPro JUCE 8 audio-plugin repo (CMake, build dir `build-debug/`).
A measured experiment proved the macOS **CVDisplayLink GPU render thread runs at ~60 fps inside
Pro Tools during playback**, while the JUCE message thread is throttled by PT to ~13 fps. We are
building a **bespoke Metal renderer for the analyzer that runs on that GPU thread**, bypassing the
message thread entirely.

Hard rules (apply to every phase):
- **macOS Metal only:** `CAMetalLayer` + `CVDisplayLink`, hosted via an `NSView` embedded in the
  JUCE editor. Objective-C++ (`.mm`) for the Metal/Cocoa glue.
- **Render thread = no JUCE message-thread dependencies:** no `MessageManager` lock, no
  `juce::Graphics`, no `Component::repaint`, no allocations in the per-frame path. Draw with Metal
  vertex buffers + shaders only.
- **Data in is lock-free:** the analyzer publishes a seqlock snapshot via
  `AnalyzerEngine::getLatestSnapshot(...)` (see `Source/analyzer/AnalyzerEngine.{h,cpp}`,
  `Source/dsp_adapters/AnalyzerSnapshotAdapter.h`). The render thread reads it directly. **Never**
  touch the audio thread or DSP.
- **Per-format behavior MUST be a runtime check** `audioProcessor.wrapperType ==
  juce::AudioProcessor::wrapperType_AAX` — **NOT** `#if JucePlugin_Build_AAX`. This is a single
  shared-code compilation where `JucePlugin_Build_AAX/VST3/Standalone` are ALL `1`, so the macro is
  true in every format. This bug already bit us once; do not reintroduce it.
- **CPU renderer stays as fallback.** All Metal work is behind a CMake flag and a runtime
  capability check. Flag off OR Metal init failure ⇒ today's CoreGraphics path, unchanged.
- **Do NOT edit `third_party/melechdsp-hq/`** until Phase B5. Build everything app-side under
  `Source/ui/analyzer/metal/`. The analyzer renderer (`RTADisplay`, `RTADisplayRenderer`,
  `BackgroundGridCache`, `RTADisplayInvalidationPolicy`) lives in that submodule for reference only.
- **Visual parity is a requirement** — match the existing CoreGraphics look (traces, fills, glow,
  grid, labels, colors, typography). Optimize by caching, never by simplifying the look.
- **Build & verify:** `cmake --build build-debug --target AnalyzerPro_Standalone` for a fast compile
  check, and `cmake --build build-debug --target AnalyzerPro_AAX` for the AAX target. You CANNOT
  test in Pro Tools or PACE-sign (needs the developer's iLok) — the human does that at each
  checkpoint. Your job per phase: implement, compile clean (no new warnings), and stop.
- A debug HUD already exists in `AnalyzerDisplayView::accumulateDiagnosticsAndMaybeHud()`; reuse its
  pattern (a `std::atomic<float>` published by the render thread, read by the message-thread HUD) to
  surface GPU-thread fps.
- A bottom-left build stamp (`AnalyzerPro vX • build <date time>`) already exists in
  `PluginEditor.cpp`; it confirms which build is loaded. Keep it working.

End of CONTEXT.

---

## Phase B0 — Metal hosting de-risk (DO THIS FIRST)

**Objective:** Prove that a self-hosted `CAMetalLayer` + `CVDisplayLink` renders at ~60 fps inside
Pro Tools during playback and survives multiple instances — BEFORE porting any real drawing. No
analyzer data yet.

**Build:**
1. Add `Source/ui/analyzer/metal/MetalProbeView.{h,mm}`: a `juce::Component` that creates/owns an
   `NSView` backed by a `CAMetalLayer`, a shared `MTLDevice`, a command queue, and a `CVDisplayLink`
   that fires a render callback on its own thread. In the callback: clear the drawable to a color
   that **cycles by frame count** (fast cycle = high fps), present, and update a frame counter →
   publish fps into a shared `std::atomic<float>` (e.g. `analyzerpro::gMetalProbeFps` in a new
   `Source/ui/analyzer/metal/MetalProbeShared.h`). Embed the NSView over the component's bounds
   (use the component's peer; handle retina `contentsScale`).
2. In `PluginEditor`: behind `#if ANALYZERPRO_METAL_PROBE`, and at runtime only when
   `wrapperType == wrapperType_AAX`, create the probe as a large centered overlay child. Detach/stop
   the `CVDisplayLink` and release Metal objects in the destructor BEFORE other teardown.
3. In `AnalyzerDisplayView` HUD: append `metalprobe_fps=<NN>` (read the atomic). Guard with the flag.
4. CMake: add `option(ANALYZERPRO_METAL_PROBE ... ON)`; when on, `target_compile_definitions(...
   ANALYZERPRO_METAL_PROBE=1)` and link `"-framework Metal" "-framework QuartzCore" "-framework
   Foundation" "-framework AppKit"`. Mark the `.mm` for ARC if needed, or manage lifetimes manually.

**Don't:** draw text in Metal (skip it — the cycling color + HUD fps are the readout). Don't touch
analyzer data, DSP, or the submodule. Don't use `#if JucePlugin_Build_AAX`.

**Acceptance (compile-time, your responsibility):** Standalone + AAX build clean; flag off ⇒ no Metal
code compiled in / no behavior change; lifetimes correct (no leaks, clean teardown ordering).

**⛔ STOP — checkpoint B0 (human verifies in Pro Tools):**
- Relaunch PT, AAX instance: centered box cycles colors; `metalprobe_fps` ≈ display rate (~60)
  **and holds during playback**.
- 8–16 instances open simultaneously + play: no crashes, no black/garbage layers, acceptable GPU
  load; close/reopen churn is clean; minimize/restore is clean.
- Report: playback fps, multi-instance stability. **Do not start B1 until this passes.** If
  self-hosted Metal misbehaves in PT, stop and report — we reassess hosting before proceeding.

---

## Phase B1 — Spectrum MVP in Metal

**Objective:** Render the FFT spectrum trace (line + fill) on the Metal/CVDisplayLink thread, reading
the lock-free snapshot, at ~60 fps during PT playback. Grid as a cached texture. Behind a flag,
AAX-first. No scopes/meters yet (those keep rendering on the CPU path as today).

**Build:**
1. Define a renderer interface `Source/ui/analyzer/metal/IAnalyzerSurfaceRenderer.h` so the editor
   can choose CPU (today) or Metal at runtime. Metal impl in
   `Source/ui/analyzer/metal/MetalAnalyzerRenderer.{h,mm}`, reusing the B0 hosting (CAMetalLayer +
   CVDisplayLink).
2. **Render-thread pipeline:** port the per-frame data processing from
   `AnalyzerDisplayView::analyzerUiTickCore()` + `AnalyzerRenderStateProvider` (dB conversion,
   weighting, octave smoothing, ballistics, peak-hold) into state **owned exclusively by the render
   thread**. Read the snapshot via `getLatestSnapshot`. The message thread publishes an immutable
   `RenderConfig` (freq range, dB window, weighting, tilt, sample rate, plot rect) to the render
   thread via `std::atomic<std::shared_ptr<const RenderConfig>>` (or a seqlocked struct) — never
   share mutable state.
3. **Geometry:** build a vertex buffer of the trace points (x = log-freq→NDC, y = dB→NDC); draw the
   fill as a triangle strip to the baseline with an alpha-gradient fragment shader, and the line as a
   width-expanded triangle strip (anti-aliased). Match current colors from the theme.
4. **Grid/labels:** for B1, render the existing CoreGraphics grid to a `juce::Image` once (reuse the
   approach in the submodule's `BackgroundGridCache`) and upload it as a Metal texture; blit it under
   the trace. Rebuild the texture only on size/theme/freq-range/dB-range change (mirror the triggers
   in `RTADisplayInvalidationPolicy`).
5. Editor: when `wrapperType == AAX` AND Metal init succeeds AND flag on, use `MetalAnalyzerRenderer`
   for the analyzer surface and **disable the CPU analyzer paint** for that surface; otherwise CPU
   path unchanged. Keep meters/scopes on CPU.
6. HUD: show Metal render-thread fps + `paint_ms` equivalent (GPU encode time).

**Acceptance:** builds clean; flag off ⇒ identical to today; the FFT trace + fill + grid look like the
CPU version at a glance.

**⛔ STOP — checkpoint B1:** Human signs/installs/tests in PT. Verify: spectrum animates at ~60 fps
during playback (HUD), trace/fill/grid match the CPU look, multi-instance stable, no DSP change.
Side-by-side a CPU build vs Metal build for parity. **Do not start B2 until confirmed.**

---

## Phase B2 — Visual parity

**Objective:** Bring the Metal analyzer to full visual parity with the CoreGraphics renderer.

**Build:** glow on the trace (2-pass blur or SDF fragment shader, tuned to match); peak trace +
peak-hold; all multi-traces (L/R/M/S/Mono/Stereo) with correct colors/blend; spectral tilt; dB-window
zoom animation; hover crosshair + Hz/dB readout (dynamic text via a small CoreGraphics→texture updated
only on change, not per frame); axis labels and any static text baked into the grid texture; bands/log
modes if applicable. Pixel-compare against the CPU renderer and close gaps.

**Acceptance:** Metal analyzer is visually indistinguishable from CPU at 100/125/150 size presets and
both themes; still ~60 fps in PT playback.

**⛔ STOP — checkpoint B2:** Human verifies parity (screenshot diff vs CPU) + fps + stability in PT.
**Do not start B3 until confirmed.**

---

## Phase B3 — Lifecycle & robustness

**Objective:** Make the Metal path production-safe.

**Build:** pause/resume the `CVDisplayLink` and Metal layer on window minimize/background/visibility
change (PT & Logic are known to assert/crash otherwise — hook peer visibility/`componentPeerChanged`);
handle display change / GPU switch / context loss with recovery; correct retina/scale and
resize/preset changes (reallocate drawable); **runtime capability check** that cleanly falls back to
the CPU renderer if Metal device/layer/display-link init fails; verify no leaks (Metal frame capture /
Instruments) across open/close churn; ensure deterministic teardown ordering (stop display link →
drain in-flight frames → release layer/device) before component destruction.

**Acceptance:** no crashes/leaks across minimize/restore, sample-rate change, display switch, 16-instance
churn; clean fallback to CPU when forced (e.g., simulate init failure).

**⛔ STOP — checkpoint B3:** Human runs the stress matrix in PT. **Do not start B4 until confirmed.**

---

## Phase B4 — Scopes & meters on GPU (optional)

**Objective:** Decide and (if worthwhile) port the goniometer, phase-fan scope, and level meters to
the Metal renderer so they also hit 60 fps during playback. These are currently the most expensive CPU
surfaces (many stroked paths/frame).

**Build:** evaluate cost/benefit first (they're secondary surfaces). If proceeding: port goniometer
(point cloud / history trails as instanced geometry), phase-fan (radial fill + contour), and meters to
Metal draw calls on the same render thread, reading their existing data sources lock-free. Keep
visual parity. If not worthwhile, document the decision and leave them on CPU.

**Acceptance:** scopes/meters either match visually at 60 fps under Metal, or a clear written rationale
for leaving them on CPU.

**⛔ STOP — checkpoint B4:** Human verifies. **Do not start B5 until confirmed.**

---

## Phase B5 — Upstream into the HQ submodule

**Objective:** Move the proven Metal renderer from app-side into the shared module so all Melech-DSP
plugins can use it, behind the `IAnalyzerSurfaceRenderer` interface.

**Build:** relocate the Metal renderer + hosting into `third_party/melechdsp-hq/shared/mdsp_gui`
(or `mdsp_ui`) alongside `RTADisplay`, behind the interface so the CoreGraphics path remains the
default/fallback. Wire AnalyzerPro to select Metal via the interface. Make per-format enablement a
runtime policy (recommend: enable for all formats once stable, since it lowers CPU everywhere; default
to AAX-on, others opt-in until validated). Update the submodule's CMake to link Metal frameworks on
macOS only. Coordinate as a separate submodule PR (it affects every consumer of the module).

**Acceptance:** AnalyzerPro uses the submodule Metal renderer; other consumers unaffected (CPU default);
submodule builds standalone; AnalyzerPro AAX/VST3/Standalone all build and behave correctly per the
runtime policy.

**⛔ STOP — checkpoint B5 (final):** Human verifies all three formats in their hosts; submodule PR
reviewed with awareness of other consumers. Then flip on per the chosen per-format policy and ship.

---

## Cross-phase reminders for Cursor
- Compile clean each phase (no new warnings); keep the CPU fallback intact and flag-gated.
- Runtime `wrapperType`, never `#if JucePlugin_Build_AAX`.
- No DSP/audio-thread edits; render thread is read-only on the lock-free snapshot.
- No `third_party/` edits before B5.
- End every phase by reporting what changed + what the human must test, then STOP.
