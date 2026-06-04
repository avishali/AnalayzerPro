# AnalyzerPro — Full-Editor Metal UI: Cursor Prompts (Phases 0–4)

Decision locked: **full-editor Metal** (Approach B — one full-window `CAMetalLayer`
composites a JUCE-rendered "chrome" texture + a GPU-drawn analyzer + input forwarding).
Companion design doc: `docs/metal_full_editor_plan.md`. Self-contained brief:
`docs/metal_ui_rewrite_HANDOFF.md`.

> **This supersedes the previous B0–B5 prompts**, which targeted a *hybrid* (Metal analyzer
> as a child view inside the JUCE editor). That model is **DEAD**: a child `CAMetalLayer`
> blanks the entire JUCE editor in Pro Tools (proven 3 ways). Do not build a child Metal
> overlay. The GPU must own the **whole** editor surface.

**How to use:** For each phase, paste **§0 CONTEXT** (below) **plus** the phase's prompt into
Cursor. Run one phase, then **STOP** at its checkpoint — sign, install, test in Pro Tools, and
confirm before moving on. Each phase leaves the plugin shippable (flag off ⇒ current CPU UI).

---

## §0 CONTEXT (prepend to every phase prompt)

You are working in the AnalyzerPro JUCE 8 audio-plugin repo (CMake, build dir `build-debug/`,
macOS / Apple Silicon, JUCE 8.0.12 at `/Users/avishaylidani/DEV/SDK/JUCE`).

**Proven facts (do NOT re-test — they cost many PT build/sign cycles):**
- During Pro Tools *playback*, PT grants the JUCE **MessageManager lock only ~13×/sec**.
  Anything painted through the JUCE component tree (CPU *or* `OpenGLContext` component
  painting) is capped at ~13 fps. Lowering paint rates does nothing — the wall is scheduling.
- The macOS **CVDisplayLink GPU render thread runs free at ~60 fps inside PT during playback**
  (measured via a blank-screen probe). It's a system display service PT cannot throttle.
- A Metal/GPU **child** view CANNOT coexist with JUCE's editor in PT — it blanks the whole
  editor (proven 3 ways, including JUCE's own Metal backing). **The GPU must own the entire
  editor surface.** A full-screen GL surface DID composite fine; full-screen *Metal* is the
  one thing still unproven (that's Phase 0).

**The architecture we are building (Approach B):** one full-window `CAMetalLayer` on the
CVDisplayLink thread composites, every frame:
1. a **chrome texture** — the entire current JUCE UI (header, control rail, footer, meters,
   scopes, loudness panel, tooltips, build stamp) rendered to an offscreen `juce::Image` at
   the message-thread rate (~13 fps in PT — fine; this UI doesn't need 60 fps), uploaded as a
   texture and drawn as a full-window quad;
2. the **analyzer spectrum**, drawn fresh from the lock-free snapshot at 60 fps via Metal
   geometry, punched in over the analyzer's plot rect.
Input is event-driven (not frame-bound), so the chrome's slow redraw doesn't hurt
responsiveness — as long as events reach JUCE's component tree (see hosting mechanisms below).

**Hard rules (apply to every phase):**
- **macOS Metal only:** `CAMetalLayer` + `CVDisplayLink`. Objective-C++ (`.mm`) for Metal/Cocoa
  glue. **ARC is OFF** — manual retain/release; `CFRelease` for CoreVideo objects.
- **Render thread = no JUCE message-thread dependencies:** no `MessageManager` lock, no
  `juce::Graphics`, no `Component::repaint`, no per-frame allocations. Draw with Metal vertex
  buffers + shaders + texture blits only.
- **Data in is lock-free:** the analyzer publishes a seqlock snapshot via
  `AnalyzerEngine::getLatestSnapshot(...)` (`Source/analyzer/AnalyzerEngine.{h,cpp}`,
  `Source/dsp_adapters/AnalyzerSnapshotAdapter.h`, `Source/analyzer/AnalyzerSnapshot.h`). The
  render thread reads it directly. **Never** touch the audio thread or DSP.
- **Message→render handoff is immutable only:** the message thread publishes an immutable
  `RenderConfig` (freq range, dB window, weighting mode, tilt, sample rate, plot rect, theme
  colors, trace enable/colors, hover x/state) and the chrome texture via
  `std::atomic<std::shared_ptr<const T>>` (or a seqlocked struct). Never share mutable state.
- **Per-format behavior MUST be a runtime check** `audioProcessor.wrapperType ==
  juce::AudioProcessor::wrapperType_AAX` — **NOT** `#if JucePlugin_Build_AAX`. This is a single
  shared-code compilation where `JucePlugin_Build_AAX/VST3/Standalone` are ALL `1`, so the
  macro is true in every format. This bug already bit twice; do not reintroduce it.
- **CPU editor stays as fallback.** All Metal work is behind the CMake flag
  `ANALYZERPRO_METAL_EDITOR` (default OFF) and a runtime capability check. Flag off OR Metal
  init failure ⇒ today's CoreGraphics editor, completely unchanged.
- **Do NOT edit `third_party/melechdsp-hq/`** until the upstreaming phase. Build everything
  app-side under `Source/ui/analyzer/metal/`. The analyzer renderer (`RTADisplay`,
  `RTADisplayRenderer`, `BackgroundGridCache`, `RTADisplayInvalidationPolicy`) lives in that
  submodule for reference/parity only.
- **Visual parity is a requirement** — match the existing CoreGraphics look (traces, fills,
  glow, grid, labels, colors, typography). Optimize by caching, never by simplifying the look.
- **Build & verify:** `cmake --build build-debug --target AnalyzerPro_Standalone` for fast
  compile checks, `... AnalyzerPro_AAX` for the AAX target. Strict warnings are on
  (`-Wconversion -Wshorten-64-to-32 -Wzero-as-null-pointer-constant -Wpedantic`) — keep builds
  warning-clean. Sources are listed explicitly in `target_sources(...)` in `CMakeLists.txt`
  (no glob) — add new files there. You **CANNOT** test in Pro Tools or PACE-sign (needs the
  developer's iLok). The human does that at each checkpoint. Your job per phase: implement,
  compile clean, and STOP.
- A debug HUD exists in `AnalyzerDisplayView::accumulateDiagnosticsAndMaybeHud()`; reuse its
  pattern (a `std::atomic<float>` published by the render thread, read by the message-thread
  HUD) to surface GPU-thread fps. A bottom-left build stamp in `PluginEditor.cpp` confirms the
  loaded build — keep it working.

End of CONTEXT.

---

## Phase 0 — Full-editor Metal hosting de-risk (DO THIS FIRST)

**Objective:** Prove a full-editor `CAMetalLayer` + `CVDisplayLink` renders at ~60 fps inside
PT during playback, survives multiple instances, and that input still reaches JUCE — BEFORE
porting any real drawing. No analyzer data yet. This phase also **picks the hosting mechanism**.

This phase builds **both** hosting mechanisms behind a sub-switch, plus a single
input-forwarding probe, so the human can compare them in PT in one signed build.

**Build:**
1. `Source/ui/analyzer/metal/MetalHostShared.h`: `std::atomic<float> gMetalHostFps`,
   `std::atomic<int> gMetalHostInputHits` (incremented when a forwarded click lands on the
   probe button), and an `enum class MetalHostMechanism { BackingLayer, CoverView }`.
2. `Source/ui/analyzer/metal/MetalHost.{h,mm}`: owns a shared `MTLDevice`, command queue,
   `CAMetalLayer`, and a `CVDisplayLink` whose callback (on its own thread) clears the drawable
   to a **frame-count-cycling color**, presents, and publishes fps into `gMetalHostFps`. Handle
   retina `contentsScale` and drawable resize. **Teardown order: stop the CVDisplayLink FIRST,
   drain in-flight frames, then release layer/queue/device.**
   - **Mechanism 1 — BackingLayer:** make the JUCE editor's top-level peer NSView layer-backed
     by our `CAMetalLayer` (`getPeer()->getNativeHandle()` → `NSView`, `wantsLayer=YES`, swap
     `view.layer`). JUCE keeps its component tree and native event routing; we only take over
     presentation. (Note: JUCE's *own* Metal backing via
     `JUCE_COREGRAPHICS_RENDER_WITH_MULTIPLE_PAINT_CALLS=1` blanked PT — do NOT enable that; we
     own the layer, JUCE must not paint to the screen layer.)
   - **Mechanism 2 — CoverView:** add our own `NSView` (hosting the `CAMetalLayer`) as the
     **topmost** subview covering the entire editor. Forward `mouseDown/Up/Dragged/Moved`,
     `scrollWheel`, and key events from the cover view into the JUCE editor: translate to JUCE
     coords and dispatch on the message thread (e.g. via the editor peer's
     `handleMouseEvent`/`handleKeyPress`, or `Component::getComponentAt` + synthesized
     `juce::MouseEvent`).
3. `PluginEditor`: behind `#if ANALYZERPRO_METAL_EDITOR` and at runtime only when
   `wrapperType == wrapperType_AAX`, instantiate `MetalHost` for the chosen mechanism. Add a
   compile-time or env/JUCE-property switch to select mechanism so the human can flip between
   them without a recompile if practical (else two builds). Add a small JUCE button somewhere
   over the surface as the **input probe**; clicking it must increment `gMetalHostInputHits`
   (this validates that events reach JUCE through whichever mechanism). Release/stop everything
   in the destructor BEFORE other teardown.
4. HUD: append `metalhost_fps=<NN> mech=<backing|cover> inputhits=<N>` (guard with the flag).
5. CMake: `option(ANALYZERPRO_METAL_EDITOR "Full-editor Metal UI" OFF)`; when ON,
   `target_compile_definitions(... ANALYZERPRO_METAL_EDITOR=1)` and link
   `"-framework Metal" "-framework QuartzCore" "-framework Foundation" "-framework AppKit"`.
   Add the new files to `target_sources`.

**Don't:** draw text in Metal; touch analyzer data, DSP, or the submodule; use
`#if JucePlugin_Build_AAX`; build a *child* Metal overlay (proven dead).

**Acceptance (compile-time, your responsibility):** Standalone + AAX build clean; flag off ⇒
no Metal code compiled, zero behavior change; lifetimes correct (clean teardown ordering).

**⛔ STOP — checkpoint 0 (human verifies in Pro Tools), for EACH mechanism:**
- Relaunch PT, AAX instance: full editor cycles colors; `metalhost_fps` ≈ ~60 **and holds
  during playback**; editor does NOT blank.
- Clicking the input-probe button increments `inputhits` (input reaches JUCE).
- 8–16 instances + play: no crashes/black layers, acceptable GPU load; close/reopen churn and
  minimize/restore are clean.
- Report which mechanism(s) composited, fps, input result, stability. **Decide the mechanism**
  (prefer BackingLayer if it works — it keeps JUCE input routing free and makes later input
  forwarding largely unnecessary). **If BOTH Metal mechanisms blank in PT**, stop and report:
  we switch the backend to a full-surface `OpenGLContext` raw-GL renderer (probe-proven) and
  re-run Phase 0. **Do not start Phase 1 until a mechanism passes.**

---

## Phase 1 — Chrome composite + analyzer MVP

> **Phase 0 result (lock this in):** the **BackingLayer** mechanism PASSED in PT — a full-editor
> `CAMetalLayer` swapped in as the editor peer's backing layer composites correctly and the
> CVDisplayLink held **~120 fps** (display rate) during playback. Input works **natively** in
> backing mode (we replaced the layer, not the event path), so blind clicks reached JUCE.
> **Therefore: build Phase 1 on BackingLayer only. Do NOT add cover-view input forwarding** —
> JUCE's native routing already works. Leave the `CoverView` path compiled but unused as a
> fallback. "~60 fps" targets below mean "display rate" (you'll see ~120 on ProMotion).

> **Critical consequence of BackingLayer:** because our `CAMetalLayer` REPLACED the editor's
> backing layer, **JUCE no longer paints to the screen** — whatever JUCE draws goes nowhere
> until we capture it ourselves and composite it. That's the whole job of Phase 1A. The flat
> color you saw in Phase 0 was the editor with no chrome composited yet.

Split Phase 1 into **1A (chrome composite)** and **1B (analyzer punch-in)** — get the real JUCE
UI back on screen *first*, prove it's interactive, then add the GPU analyzer. Each sub-phase is
its own ⛔ STOP. Both behind the flag, AAX-first, BackingLayer.

### Phase 1A — Composite the real JUCE UI as a texture (no analyzer yet)

**Objective:** Replace the flat clear color with the actual editor UI, rendered by JUCE to an
offscreen image and drawn by Metal as a full-window quad. The UI must look and behave exactly
like today (it's just being presented through Metal instead of directly).

**Build:**
1. `Source/ui/analyzer/metal/IEditorSurface.h` — interface so the editor picks CPU (today) or
   Metal at runtime. Metal impl `MetalEditorRenderer.{h,mm}` reusing `MetalHost` (BackingLayer).
2. **Chrome capture (message thread):** each message-thread tick (and on resize/theme change),
   render the editor's component tree to an offscreen `juce::Image` (ARGB, backing-scale-aware).
   Prefer painting the editor's children directly into an `Image`'s `Graphics` (e.g.
   `comp.paintEntireComponent (g, false)` per top-level child, or `createComponentSnapshot` of
   `mainView`) — do NOT rely on the on-screen path, which no longer reaches the display.
3. **Handoff (immutable, double-buffered):** publish the captured image to the render thread via
   `std::atomic<std::shared_ptr<const FrameTexturePayload>>` (image pixels + width/height/scale +
   a dirty/sequence counter). Render thread uploads to an `MTLTexture` only when the sequence
   changes; otherwise reuses the last texture. **No `juce::Graphics`/allocations on the render
   thread.**
4. **Composite:** render thread draws the chrome texture as a full-window quad each frame (this
   is the only content in 1A). Drop the cycling-color clear.
5. Editor selection: when `wrapperType == AAX` AND flag on AND Metal init succeeds → Metal
   surface; else today's CPU editor, unchanged. Keep the build stamp working.
6. **Per-instance diagnostics:** the Phase-0 globals (`gMetalHostFps/InputHits`) are
   process-shared and get stomped when multiple editor windows are open. Give each `MetalHost`
   an **instance id** (atomic counter at construction) and include it in the NSLog line:
   `[MetalHost #<id>] mech=backing fps=<N>`. This makes the multi-window stress test readable.

**Acceptance (compile-time):** builds clean; flag off ⇒ identical to today.

**⛔ STOP — checkpoint 1A (human in PT):** the **full AnalyzerPro UI renders** (header, rail,
meters, scopes, loudness, footer, build stamp) via the Metal composite, visually matching a CPU
build; mouse/keyboard/combos/menus/rail-scroll all work; per-instance fps logs show display rate;
open **several editor windows at once** + play → no crashes/black layers (the multi-window stress
Phase 0 couldn't show). **Do not start 1B until confirmed.**

### Phase 1B — GPU analyzer punch-in

> **State after 1A (verified in PT):** full JUCE editor composites through Metal at ~120 fps via
> BackingLayer; `MetalHostImpl::renderFrame()` currently **blits** the chrome texture
> (`blitCommandEncoder` → `copyFromTexture:toTexture:`) onto the drawable. Native input + hover
> work. The analyzer trace is still CPU-painted **inside** the chrome texture, so it only updates
> at the ~13 fps PT message-thread cap during playback. 1B is the perceptual payoff: draw the
> spectrum as GPU geometry on the 120 fps render thread.

**Objective:** Draw the FFT spectrum on the render thread, over the analyzer's plot rect, on top
of the composited chrome — at display rate during playback. Keep everything else exactly as 1A.

**Two structural changes 1B forces (call these out — they're not optional):**

- **(A) Blit → render pass.** A `blitCommandEncoder` is a raw texture copy; it CANNOT draw
  geometry on top. Replace the chrome blit in `renderFrame()` with a **render pass**: a render
  pipeline that (1) draws the chrome texture as a **sampled full-surface quad** (two triangles,
  a `texture2d` sampler, `MTLLoadActionDontCare`/clear), then (2) draws the analyzer geometry in
  the **same** render encoder over the plot rect. Add a `.metal` shader source (textured-quad +
  line/fill shaders) compiled into the default library; build the `MTLRenderPipelineState`s once
  at init, not per frame. Keep the old blit path compiled as a fallback if pipeline build fails.
- **(B) Land the latent 1A scale fix now.** Capture and drawable must use ONE scale. Today
  capture uses `editor->getDesktopScaleFactor()` and the drawable uses `window.backingScaleFactor`
  — they only matched in 1A because the test display was 1×. Unify on the host's
  `backingScaleFactor` (expose it from `MetalHost`; capture the chrome image at
  `editorSize × backingScale` = the drawable pixel size). The analyzer plot rect and all geometry
  must be computed in the **same physical-pixel space** as the drawable.

**Build:**
1. **Suppress only the moving trace in the chrome capture** (not the grid/labels): add a flag to
   `AnalyzerDisplayView`/`RTADisplay` so, when the Metal surface is active, the **spectrum
   trace+fill (RMS/Peak/Hold)** paint is skipped while the grid, axis labels, background, and
   hover crosshair still paint into the chrome. This keeps the static grid at chrome rate (fine)
   and lets the GPU draw only the fast-moving trace — the minimal change for the smoothness win
   and the least shader/parity risk. Flag OFF or non-Metal ⇒ CPU analyzer paints normally
   (permanent fallback). *(If the trace and grid can't be separated in `RTADisplay`, fall back to:
   suppress the whole analyzer + GPU-draw a cached `BackgroundGridCache` grid texture under the
   trace, rebuilt only on `RTADisplayInvalidationPolicy` triggers.)*
2. **Plot rect handoff:** expose the analyzer's plot rect in **editor coordinates**, converted to
   physical pixels via the unified backing scale, into the published `RenderConfig`, so the render
   thread knows where to draw and clip.
3. **Render-thread analyzer pipeline:** port the per-frame processing from
   `AnalyzerDisplayView::analyzerUiTickCore()` + `mdsp::gui::AnalyzerRenderStateProvider` (dB
   conversion, **power-domain** weighting *before* octave smoothing, 1/3-octave smoothing,
   ballistics, peak-hold; internal dB floor −200, `sanitizeDb` clamp [−200, 24]) into state
   **owned exclusively by the render thread**. Read the snapshot via `getLatestSnapshot`. The
   message thread publishes the immutable `RenderConfig` (freq range, dB window, weighting, tilt,
   sample rate, plot rect, theme/trace colors) via the existing `atomic<shared_ptr>` pattern.
   Build vertex data on the render thread with NO allocations in steady state (preallocate buffers
   sized to max bins; reuse).
4. **Analyzer geometry:** vertex buffer of trace points (x = log-freq→NDC over the plot rect,
   y = dB→NDC); fill as a triangle strip to baseline with an alpha-gradient fragment shader; line
   as a width-expanded, anti-aliased triangle strip. Clip/scissor to the plot rect. Start with the
   main RMS trace for first-light; add Peak/Hold once RMS matches. Colors from `RenderConfig`.
5. HUD/log: add render-thread GPU encode time (ms) alongside fps; keep the per-instance
   `[MetalHost #N]` line.

**Don't:** keep the blit for the analyzer layer (it can't composite geometry); allocate per frame
on the render thread; touch DSP/audio; edit `third_party/`; use `#if JucePlugin_Build_AAX`. Keep
the CPU analyzer paint intact behind the suppression flag as the permanent fallback.

**Acceptance (compile-time):** Standalone + AAX build clean; flag off ⇒ identical to today; main
trace + fill look like the CPU version at a glance; render pass replaces the blit with the chrome
still compositing correctly.

**⛔ STOP — checkpoint 1B (human in PT):** the spectrum **animates at display rate during
playback** (compare the on-screen smoothness to a CPU build — this is THE payoff, distinct from
1A's present-rate); `render_fps` for the analyzer is no longer the gate (the GPU trace is
decoupled from it); trace/fill match the CPU look; grid/labels/rest-of-UI unchanged and
interactive; the scale fix holds on BOTH a 1× and a 2× (Retina) display — verify the UI fills the
editor on the laptop's built-in panel, not just the 1× external; multi-window stable; no DSP
change. **Do not start Phase 2 until confirmed.**

---

### Phase 1B — follow-up: move the analyzer pipeline onto the render thread (REQUIRED — 1B objective unmet without it)

**Why:** 1B's draw path is correct (render pass, GPU geometry, scale unified, trace suppression,
preallocated buffers — keep all of it) but the **data path runs on the message thread**, so it
does NOT achieve smooth playback. Today `MetalAnalyzerFrame` carries a pre-computed
`std::vector<float> rmsDb` filled by `fillMetalAnalyzerFrame()` from `MetalEditorRenderer`'s
`juce::Timer` (message thread). During PT playback that timer is starved to ~13 Hz, so the
spectrum data only changes ~13×/sec while the render thread redraws the *same* data at 120 fps.
`metalhost_fps=120` is therefore misleading — the trace is as choppy as the CPU path. The render
thread currently never calls `getLatestSnapshot`. Fix: the render thread must read the lock-free
snapshot and run the pipeline itself, every frame.

**Core idea (this is what creates smoothness):** the engine publishes new FFT spectra at ~30/sec
(`data_fps`), but **ballistics/smoothing run per render frame** toward the latest snapshot, so a
120 fps render thread produces smoothly *interpolated* motion between data updates. The smoothness
comes from running ballistics at display rate, not from raw FFT rate. So the pipeline (incl.
ballistics + peak-hold decay) must execute on the render thread every frame.

**Change:**
1. **Render thread reads the snapshot.** Give the render side access to the engine
   (`AnalyzerEngine&` is reachable from the editor as `analyzerModule`). Pass an engine pointer (or
   a lock-free `std::function<bool(SnapshotOut&)>` that wraps `getLatestSnapshot`) into
   `MetalHost`/the renderer at `start()`. Each `renderFrame()`: call `getLatestSnapshot(...)`
   (`Source/analyzer/AnalyzerSnapshot.h`, `AnalyzerSnapshotAdapter.h`) — it's a lock-free seqlock,
   safe off the message thread. **Never touch the audio thread or any UI/component object from the
   render thread.**
2. **Port the pipeline to render-thread-owned state.** Move a copy of the per-frame processing
   from `AnalyzerDisplayView::analyzerUiTickCore()` + `mdsp::gui::AnalyzerRenderStateProvider`
   (dB conversion, **power-domain weighting BEFORE 1/3-octave smoothing**, octave smoothing,
   ballistics, peak-hold) into a thread-agnostic processor instance **owned exclusively by the
   render thread** — its smoothing history / ballistics / peak-hold state is private to that
   thread, never shared with the message-thread CPU instance. Reuse the existing DSP math; do not
   reinvent it. Honor: internal dB floor −200, `sanitizeDb` clamp [−200, 24], and the existing
   main-RMS ballistics behaviour (the engine snapshot already carries engine-side ballistics; this
   replicates the UI-side ballistics layer that runs message-side today).
3. **Rate-correct the time constants.** Ballistics and peak-hold decay are time-based and the
   current code assumes the ~30 Hz UI tick. On the 120 Hz render thread, recompute attack/release/
   decay per frame from the **real elapsed time** (use the `CVTimeStamp` delta passed to the
   display-link callback, or `CACurrentMediaTime()` delta). Otherwise attack/release will run ~4×
   too fast. Verify decay visually matches the CPU path.
4. **Message thread publishes CONFIG ONLY.** `MetalAnalyzerFrame` (or a renamed `RenderConfig`)
   should carry the immutable config — plot rect (px), min/max Hz, top/bottom dB, weighting mode,
   tilt, sample rate, fftSize, trace colors, enabled traces — and **NOT** `rmsDb`. Drop the
   message-thread `fillMetalAnalyzerFrame` spectrum computation; keep publishing config on change/
   tick via the existing `atomic<shared_ptr>` handoff. The render thread reads config (atomic) +
   snapshot (seqlock), runs the pipeline, fills the preallocated vertex buffers.
5. **No per-frame allocations on the render thread.** Preallocate all pipeline scratch + the
   smoothed-output buffer (sized to max bins) once; reuse every frame. Keep the existing
   preallocated `analyzerFillVertices` / `analyzerLineVertices`.
6. **RMS first.** Get the main RMS trace smooth at display rate during playback before porting
   Peak/Hold (those are Phase 2). Keep the CPU analyzer + trace-suppression fallback intact.

**Don't:** compute the spectrum on the message thread; read the snapshot only on the message
thread; share mutable pipeline state between threads; allocate per frame on the render thread;
touch DSP/audio or `third_party/`; use `#if JucePlugin_Build_AAX`.

**Acceptance (compile-time):** Standalone + AAX build clean; flag off ⇒ unchanged.

**⛔ STOP — checkpoint 1B-follow-up (human in PT):** during **playback**, the RMS trace animates
**smoothly at display rate** — visibly smoother than a CPU build, and no longer gated by the
~13 fps message-thread `render_fps`. Ballistics attack/release look the same as the CPU path (not
too fast). Trace value tracks the signal correctly (parity with CPU at a glance). Rest of UI
unchanged/interactive; multi-window stable; no DSP change. This is THE smoothness payoff — confirm
it before Phase 2.

---

### Phase 1B — follow-up 2: fix chrome-capture cost (PT GUI slowdown) + flashing/missing frames

Two PT-confirmed issues. The render-thread analyzer pipeline is correct — do **not** change it.
Fix the chrome plumbing only.

**Issue ① — PT's whole GUI slows down when the window is open during playback.** Cause: the
chrome capture is too expensive on the message thread (= PT's main thread in AAX). In
`MetalEditorRenderer`:
1. **Stop allocating per capture.** Today `captureChromeFrame()` does
   `make_shared<FrameTexturePayload>()` + `bgraPixels.resize(...)` every tick (multi-MB alloc +
   free each time). Replace with a small **pool** of 2–3 reusable payload buffers (preallocated,
   resized only on editor-size change) rotated by the publish; never allocate in steady state.
2. **Lower the capture rate.** Drop `kChromeCaptureHz` from 30 to **15** (meters/scopes at 15 fps
   are fine — secondary surfaces). Roughly halves the main-thread paint+copy cost during playback.
3. Keep `paintEntireComponent`, but skip the capture entirely when the editor is not
   showing/visible.

**Issue ② — trace flashing / dropped frames.** Two causes in `MetalHost`:
4. **Double/triple-buffer the chrome texture.** Today one `chromeTexture` is `replaceRegion`-written
   while the render pass samples it → CPU/GPU race → tearing/flash. Use a ring of **3** chrome
   textures: upload each new payload into the next free slot, atomically publish the current index;
   the render pass samples the published slot. (3 slots @ ~15 Hz uploads vs 120 Hz reads guarantees
   the GPU finished a slot long before reuse.)
5. **Never blank the trace on a snapshot miss.** In `drawAnalyzerFrame`/
   `updateAnalyzerPipelineFromSnapshot`, when `getLatestSnapshot` fails or returns ≤1 bins, **keep
   the last good `analyzerSmoothedDb` and last `validBins`** and redraw them instead of `return`-ing
   (skip only if never primed). The trace holds steady, not vanishes, on a transient read miss.

**Optional (only if ① persists after 1–2):** cap the render/present rate to ~60 fps (skip alternate
display-link callbacks) — 120 fps doubles GPU + WindowServer composite cost for no perceptual gain
over 60. Gate behind a constant so it's easy to toggle.

**Don't:** change the render-thread analyzer pipeline, ballistics, or snapshot read; allocate per
frame on either thread in steady state; touch DSP/`third_party/`; use `#if JucePlugin_Build_AAX`.

**Acceptance (compile-time):** Standalone + AAX build clean; flag off ⇒ unchanged.

**⛔ STOP — checkpoint (human in PT):** during playback, (a) PT's own GUI/meters no longer slow down
with the window open; (b) the analyzer trace is steady — no flashing/dropped frames; (c) trace still
animates smoothly and matches CPU feel; multi-window stable.

---

### Phase 1B — follow-up 3: chrome capture hangs PT (main-thread saturation) — make it cheap + bounded

**PROVEN by sampling a hung Pro Tools (PID main-thread stack):** the message-thread chrome capture
saturates PT's main thread and hangs the host. Stack:
`MetalEditorRenderer::timerCallback → captureChromeFrame → paintEntireComponent →
StereoScopeComponent::paint → strokePath` (dominant leaf), plus `PhaseFanScopeComponent::paint →
strokePath` and the full-window pixel copy (`MetalEditorRenderer.mm:135`). One full-editor capture
exceeds the timer interval, the timer re-fires back-to-back, the main thread never returns to PT's
run loop → **PT hangs**, and the render thread is so starved it never starts → **blank screen, no
fps logs**. The cost driver is the two **animated vector scopes** re-stroked every capture, made ~4×
worse at 2× Retina. The render-thread analyzer pipeline is fine — **do NOT change it. Fix the chrome
capture only.**

**The structural fix (this one is mandatory — it makes the hang impossible regardless of cost):**
1. **Self-rescheduling, time-boxed capture — never a fixed-rate Timer.** Replace the fixed
   `startTimerHz` with a **one-shot timer that reschedules itself only after each capture
   completes**. Measure each capture's wall time (`CACurrentMediaTime` around paint+copy). Schedule
   the next capture at `interval = max(baseIntervalMs, lastCaptureMs * K)` with `K ≈ 4–5`, so chrome
   capture can never consume more than ~20–25% of the main thread and **can never run back-to-back**.
   Base rate ~**10 Hz**. This alone prevents the hang even if a capture is expensive.
2. **Capture at logical (1×) resolution; let the GPU upscale.** Decouple the chrome texture
   resolution from the drawable's backing scale: capture the editor at **logical pixels (scale = 1)**
   regardless of `backingScale`, upload that smaller texture, and let the existing textured-quad
   render pass sample it (linear filter) to fill the full-res drawable. Cuts scope-stroke + pixel-copy
   cost ~4× on Retina. The **GPU analyzer keeps drawing at full drawable resolution** (stays sharp);
   only the chrome is upscaled (slightly soft meters/text — acceptable for now). **Important:** keep
   the analyzer plot-rect/geometry in **drawable-pixel space** (full res) — do NOT conflate it with
   the now-1× chrome capture space. The chrome quad just fills NDC −1..1; the analyzer geometry uses
   drawable px. (This re-introduces a capture/drawable scale *difference* on purpose, but it's handled
   by the sampler scaling the full-surface quad, not a 1:1 blit — verify the chrome fills the whole
   editor, no quarter-render.)
3. **Fix the visibility guard:** the `isShowing()` gate did NOT cause this (capture was running), but
   replace `isShowing()/isVisible()` with `getPeer() != nullptr` so a genuinely hidden/minimized
   window skips cleanly without depending on JUCE's host-unreliable `isShowing()`.

**Strategic note (read, may affect Phase ordering):** even after 1–3, two continuously-animated
vector scopes re-rendered into the chrome every capture is the heavy item. If 1–3 don't get PT
comfortably responsive, the durable fix is to **move the scopes (goniometer + phase-fan) to GPU
rendering (Phase 4) before shipping**, or capture them on a much slower separate cadence than the
rest of the chrome. Surface a recommendation after testing 1–3.

**Don't:** change the render-thread analyzer pipeline/ballistics/snapshot read; use a fixed-rate
timer for capture; allocate per frame; touch DSP/`third_party/`; use `#if JucePlugin_Build_AAX`.

**Acceptance (compile-time):** Standalone + AAX build clean; flag off ⇒ unchanged.

**⛔ STOP — checkpoint (human in PT):** (a) PT no longer hangs and stays responsive with the window
open + playing; (b) the full UI renders (chrome via upscaled texture — accept slight softness), no
quarter-render; (c) the analyzer trace is sharp and smooth at display rate; (d) `encode_ms` and the
adaptive capture interval are sane (capture not dominating the main thread); multi-window stable.

---

### Phase 1B — follow-up 4 (Phase 3 lifecycle): fix the CVDisplayLink teardown DEADLOCK on editor close

**Status:** follow-up 3 FIXED the capture-saturation hang (main thread is no longer stuck in
`paintEntireComponent`). The plugin now opens/renders. But closing the editor **deadlocks PT every
time.** This is the Phase-3 lifecycle hazard; it must be fixed before the build is usable.

**PROVEN by sampling the deadlocked PT (two-thread deadlock):**
- **Main thread:** `~MetalEditorRenderer → MetalHost::stop() (MetalHost.mm:273) → CVDisplayLinkStop()`
  → `__psynch_mutexwait` — blocked waiting for the in-flight render callback to finish.
- **CVDisplayLink render thread:** `displayLinkCallback → renderFrame() (MetalHost.mm:750) →
  @autoreleasepool pop → objc_release(CAMetalDrawable)` — blocked releasing the just-presented
  drawable.

**Mechanism:** `CVDisplayLinkStop()` on the main thread blocks until the current render callback
returns. That callback is blocked releasing the presented `CAMetalDrawable`, whose present
completion needs a **CoreAnimation transaction commit that runs on the main thread's run loop**. The
main thread is parked in `CVDisplayLinkStop()`, so the CA commit never happens → the drawable never
finishes presenting → the callback never returns → `CVDisplayLinkStop()` never returns. Deadlock,
main ⟷ render via the main run loop. (Classic CVDisplayLink+CAMetalLayer teardown deadlock — the
synchronous stop is happening during destruction while the main run loop is frozen.)

**Fix — stop+drain the display link while the run loop is still alive, NOT in the destructor:**

1. **Primary fix: pause/stop the render loop on visibility / peer loss, before destruction.** Hook
   the editor losing its peer / being removed from the desktop (e.g. `componentPeerChanged` when
   `getPeer() == nullptr`, and/or `componentParentHierarchyChanged` / `componentVisibilityChanged`
   to hidden). Stop + drain the CVDisplayLink THERE. At that point JUCE/PT is closing the window but
   the **main run loop is still pumping CA transactions**, so the in-flight present completes, the
   callback exits, and `CVDisplayLinkStop()` returns cleanly. By the time `~MetalHostImpl` runs, the
   link is already stopped → the destructor `stop()` is a cheap no-op. Pause on hidden, resume
   (restart link) on shown, so tab/minimize cycles work too.
2. **Make the render callback exit fast when stopping.** First line of `renderFrame()`: if
   `stopping` (acquire) is set, `return` immediately — do NOT call `nextDrawable`/present. Ensures no
   new present starts once teardown begins.
3. **Don't let the main thread synchronously block on an in-flight present.** Reorder `stop()`:
   set `stopping = true`; **then `CVDisplayLinkStop()`**; the drain loop (`while inFlightFrames > 0`)
   stays — but because (1) already stopped+drained at visibility-loss while the run loop was alive,
   the destructor path has nothing in flight. As a defensive measure, the drain spin should pump
   briefly / have a bounded timeout so it can never hang forever even if a present is stuck.
4. **Defensive:** set `metalLayer.allowsNextDrawableTimeout = YES` (default, but set it explicitly);
   consider presenting via a command-buffer completion handler and releasing the drawable there,
   rather than relying on the `@autoreleasepool` pop on the render thread, so drawable release is
   never on the teardown-critical path.
5. **Idempotency:** `stop()` and the visibility-stop must be safe to call repeatedly and in any order
   (guard on `running`/`displayLink != nullptr`).

**Don't:** change the render-thread analyzer pipeline; call `CVDisplayLinkStop()` from inside the
display-link callback; touch DSP/`third_party/`; use `#if JucePlugin_Build_AAX`.

**Acceptance (compile-time):** Standalone + AAX build clean; flag off ⇒ unchanged.

**⛔ STOP — checkpoint (human in PT):** open AND **close** the editor repeatedly — no hang on close;
minimize/restore and (if a tabbed/hidden state exists) hide/show cycles are clean; close PASTE the
plugin / delete the track — no hang; 8–16 instance open/close churn — no hang/leak; the analyzer
still renders + is smooth while open. This is THE gate that makes the build usable.

---

### Phase 1B — follow-up 5 (Phase 3 lifecycle): migrate render driver CVDisplayLink → CADisplayLink

**Why (researched, canonical):** the teardown deadlock is the well-documented `CVDisplayLinkStop()`
problem — it's synchronous and blocks the main thread on the IO-thread's internal mutex while the
in-flight render callback is blocked releasing the presented `CAMetalDrawable` (which needs a
main-thread CoreAnimation commit — amplified because BackingLayer makes our layer the view's backing
layer). Confirmed by two PT samples: main `…componentVisibilityChanged → stopDisplayLinkAndDrain →
CVDisplayLinkStop → mutexwait`; render `renderFrame → autoreleasepool pop → objc_release(drawable)`.
"Stop earlier / completion-handler" did NOT fix it. macOS 14+ **deprecated CVDisplayLink** for
AppKit `displayLink(target:selector:)` which vends a `CADisplayLink`; `invalidate()` is NOT the
blocking IO-mutex call `CVDisplayLinkStop` is, and AppKit-managed links **auto-suspend when the view
leaves a display**. We're on macOS 15+ (Darwin 25). Migrate the render driver; keep everything else.

**Scope:** change ONLY the render-driver layer in `MetalHost.mm` (how the per-frame callback is
scheduled + torn down). Do NOT change `renderFrame()`'s body, the render pass, the analyzer
render-thread pipeline, chrome capture, or BackingLayer hosting.

**Do:**
1. **Replace `CVDisplayLink*` with a `CADisplayLink`** obtained from the editor's peer `NSView`:
   `[peerView displayLinkWithTarget:proxy selector:@selector(step:)]` (ObjC). Use a tiny ObjC proxy
   object whose `-(void)step:(CADisplayLink*)l` calls `impl->renderFrame()`. Get/create the link at
   attach time (when the peer exists), invalidate at detach.
2. **Run it off the main thread.** Create a dedicated render thread that sets up its own run loop;
   `[displayLink addToRunLoop:thatRunLoop forMode:NSRunLoopCommonModes]`; run the run loop
   (`CFRunLoopRun`). The callback fires on that thread → independent of PT's starved main thread.
   (If an AppKit-managed link refuses to drive a non-main run loop at full rate, fall back to
   `NSScreen.displayLinkWithTarget:selector:` for the current screen on the render thread.)
3. **Teardown that cannot deadlock:** on peer-loss/hidden/destroy → set `stopping`, `[displayLink
   invalidate]`, then `CFRunLoopStop` the render run loop and let the thread exit. **Never block the
   main thread waiting on an in-flight present.** `invalidate()` does not wait on the callback the
   way `CVDisplayLinkStop` does. If you must join the render thread, ensure the in-flight present has
   already completed (see #4) so the join can't deadlock; otherwise detach and let it self-exit, with
   the render context kept alive (shared ownership) until the callback can no longer run (no UAF).
4. **Decouple present from the main run loop (belt-and-suspenders for BackingLayer):** wrap the
   drawable present in an explicit transaction on the render thread —
   `[CATransaction begin]; … [commandBuffer presentDrawable:drawable]; [commandBuffer commit];
   [CATransaction commit];` — so the present/commit doesn't depend on the main thread's implicit CA
   transaction. Keep `allowsNextDrawableTimeout = YES` and the early `stopping` bail.
5. Keep per-instance fps logging; add a one-time log of the driver (`CADisplayLink`) + the measured
   off-main fps so the gate below is verifiable.

**Don't:** call any `CVDisplayLink*` API; block the main thread in teardown; change the analyzer
pipeline / render pass / chrome capture / BackingLayer; use `#if JucePlugin_Build_AAX`.

**Acceptance (compile-time):** Standalone + AAX build clean; flag off ⇒ unchanged.

**⛔ STOP — checkpoint (human in PT) — TWO gates:**
- **(A) Foundational re-validation:** with the window open + **playing**, the CADisplayLink callback
  holds ~display rate (~60–120 fps) **off the main thread** (Console fps log) — i.e. the
  off-main-thread render-rate property still holds with the new driver. If it collapses to ~13 fps,
  the link is firing on the main run loop — stop and report (we re-evaluate the render-thread setup).
- **(B) Teardown:** open AND **close** repeatedly, minimize/restore, delete track/plugin,
  8–16-instance open/close churn — **no hang, no crash**. This is the bug we're fixing.

**Objective:** Bring the Metal analyzer to full visual parity with the CoreGraphics renderer.

**Build:** glow on the trace (2-pass blur or SDF fragment shader, tuned to match); peak trace +
peak-hold; all multi-traces (L/R/M/S/Mono/Stereo) with correct colors/blend (user colors from
`TraceColorStore` via `RenderConfig`); spectral tilt; dB-window zoom animation (interpolated
render-thread-side); hover crosshair drawn on GPU from `RenderConfig.hover`, with the Hz/dB
readout rendered message-thread-side into a small texture updated only on change (off the hot
path); axis labels and static text baked into the grid texture; band/log modes if applicable.
Pixel-compare against the CPU renderer and close gaps.

**Acceptance:** Metal analyzer is visually indistinguishable from CPU at 100/125/150 size
presets and both themes; still ~60 fps in PT playback.

**⛔ STOP — checkpoint 2:** Human verifies parity (screenshot diff vs CPU) + fps + stability in
PT. **Do not start Phase 3 until confirmed.**

---

## Phase 3 — Lifecycle & robustness

**Objective:** Make the Metal editor production-safe.

**Build:** pause/resume the `CVDisplayLink` + Metal layer on window minimize/background/
visibility change (PT & Logic assert/crash otherwise — hook peer visibility/
`componentPeerChanged`); handle display change / GPU switch / context loss with recovery;
correct retina/scale and resize + AAX size-preset (100/125/150) changes (reallocate drawable +
rebuild chrome texture at new size); **runtime capability check** that cleanly falls back to the
CPU editor if Metal device/layer/display-link init fails; verify no leaks (Metal frame capture /
Instruments) across open/close churn; deterministic teardown (stop display link → drain frames →
release layer/device) before component destruction. If Phase 0 chose CoverView, fully harden
the input forwarding here (hover enter/exit, drag capture, right-click, double-click, modifiers,
wheel/trackpad, keyboard focus, and that menus/tooltips — separate `NSWindow`s — composite over
Metal).

**Acceptance:** no crashes/leaks across minimize/restore, sample-rate change, display switch,
resize/preset, 16-instance churn; clean fallback to CPU when init failure is simulated; input
fully functional.

**⛔ STOP — checkpoint 3:** Human runs the stress matrix in PT. **Do not start Phase 4 until
confirmed.**

---

## Phase 4 — Scopes/meters decision, per-format enablement, submodule upstream

**Objective:** Finalize secondary surfaces, widen format coverage, and move the proven renderer
into the shared module.

**Build:**
1. **Scopes/meters decision:** by default they ride in the chrome texture (message-thread rate —
   fine). If they feel laggy in PT playback, port goniometer (`StereoScopeComponent`) +
   phase-fan (`PhaseFanScopeComponent`) to GPU geometry (they already have lock-free
   `StereoScopeRenderStateProvider` / `PhaseFanRenderStateProvider` in `mdsp_ui/scopes` — same
   handoff pattern as the analyzer). Meters are cheap bars — likely stay in chrome. Document the
   decision either way.
2. **Per-format enablement:** make format gating a runtime policy. Recommend enabling all
   formats (Metal lowers CPU everywhere); default AAX-on, VST3/AU/Standalone opt-in until
   validated, then flip on.
3. **Upstream:** relocate the Metal renderer + hosting into
   `third_party/melechdsp-hq/shared/mdsp_gui` (or `mdsp_ui`) behind the `IEditorSurface`
   interface so the CoreGraphics path stays default/fallback. Update the submodule CMake to link
   Metal frameworks on macOS only. This is a **separate submodule PR** — it affects every
   Melech-DSP plugin; coordinate accordingly.

**Acceptance:** scopes/meters either match at 60 fps under Metal or have a written rationale for
staying in chrome; AnalyzerPro uses the submodule renderer; other consumers unaffected (CPU
default); all three formats build and behave per the runtime policy.

**⛔ STOP — checkpoint 4 (final):** Human verifies all formats in their hosts; submodule PR
reviewed with awareness of other consumers. Then flip on per the chosen policy and ship.

---

## Cross-phase reminders for Cursor
- Compile clean each phase (no new warnings); keep the CPU fallback intact and flag-gated
  (`ANALYZERPRO_METAL_EDITOR`, default OFF).
- Runtime `wrapperType`, never `#if JucePlugin_Build_AAX`.
- The GPU owns the WHOLE surface — never a child Metal overlay (proven to blank PT).
- Message→render handoff is immutable (atomic<shared_ptr>/seqlock); render thread owns its
  pipeline state; no MM lock / `juce::Graphics` / allocations on the render thread.
- No DSP/audio-thread edits; render thread is read-only on the lock-free snapshot.
- No `third_party/` edits before the upstream phase. ARC is OFF in `.mm`.
- End every phase by reporting what changed + what the human must test, then STOP.
