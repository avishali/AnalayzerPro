# HANDOFF — AnalyzerPro Full-Editor Metal UI Rewrite

Purpose: brief a fresh session to plan and build a **full-editor GPU (Metal) UI** for
AnalyzerPro so the analyzer renders at ~60 fps during Pro Tools playback. This doc is
self-contained — it captures the hard-won findings (many build cycles) so you don't
re-derive them. **Next step after reading: build the full rewrite plan.**

---

## 1. Mission

AnalyzerPro's AAX UI is sluggish during Pro Tools playback. Root cause is proven (below):
Pro Tools starves the message thread to ~13 fps during playback, and JUCE component
rendering (CPU *or* GPU-via-component-painting) is gated by that. The **GPU display-link
thread runs free at 60 fps in PT** — so the fix is to render the editor with a bespoke
Metal renderer on that thread. A CPU interim build is already shipped; this is the
follow-on project to get flawless playback.

Goal of the new session: **a concrete, phased plan to rewrite the AnalyzerPro editor UI in
Metal** (full-editor, because a Metal sub-view cannot coexist with JUCE in PT — proven).

---

## 2. Hard-won empirical findings (DO NOT re-test — these cost many build/sign/PT cycles)

1. **PT message-thread cap.** During Pro Tools *playback*, every `juce::Timer`/repaint is
   serviced ~13×/sec regardless of paint cost. Measured: analyzer `paint_ms ≈ 3.7 ms` but
   the timer fires ~75 ms apart. Idle/stopped → full rate. So the wall is *scheduling*, not
   compute (CPU was ~6%).
2. **Lowering rates doesn't help.** Halving scope/meter rates (30→15) changed playback fps by
   nothing. It's not aggregate paint load.
3. **VBlankAttachment is also starved in PT** (its async callback marshals to the message
   thread). Was the original sluggishness cause (~2–13 fps). AAX now uses a plain `juce::Timer`
   (idle went 1.8 → 24 fps). VST3/AU/Standalone keep VBlank (works fine there).
4. **The free GPU thread runs at 60 fps in PT — PROVEN.** A blank-screen probe
   (`juce::OpenGLContext` attached to the **top-level editor**, `setComponentPaintingEnabled
   (false)`, custom `renderOpenGL`, NO MessageManager lock) measured **~57–60 fps holding
   during playback**. CVDisplayLink is a system display service PT cannot throttle.
5. **JUCE's `OpenGLContext` component-painting is the WRONG tool.** It (a) still needs the
   MessageManager lock for `paintComponent` → gated to ~13 fps, and (b) CPU-tessellates vector
   paths → 22–50 ms/frame (vs 3.7 ms CoreGraphics). So "turn on the GL renderer" is both
   gated and slow. The win requires a *custom* renderer drawing GPU geometry, no MM lock.
6. **A Metal/GPU CHILD view cannot coexist with JUCE's editor in Pro Tools — PROVEN 3 WAYS.**
   A `CAMetalLayer` embedded via `juce::NSViewComponent` (300×90 child overlay) **blanked the
   entire JUCE editor** in PT (only host chrome rendered). Did not help: (a) proactively
   layer-backing the editor peer (`wantsLayer=YES`), (b) enabling JUCE's own Metal-layer
   editor rendering (`JUCE_COREGRAPHICS_RENDER_WITH_MULTIPLE_PAINT_CALLS=1`). All three blanked.
   → **The incremental "Metal analyzer + JUCE meters/scopes" hybrid is DEAD in PT.**
7. **GPU owning the WHOLE surface composites fine.** The full-screen GL blank probe rendered
   correctly at 60 fps in PT. (Note: full-screen *Metal* specifically was never tested — child
   Metal blanked before we got there. **First de-risk in the new plan: confirm a full-editor
   `CAMetalLayer` composites in PT.** High confidence given full-screen GL worked, but unproven.)
8. **CPU paint is cheap and not the problem** (3.7 ms). Don't waste effort on CPU paint
   micro-opt — it can't lift the host cap.

Conclusion: the only path to 60 fps playback in PT is **the GPU owning the entire editor
surface** with a custom Metal renderer on the CVDisplayLink thread.

---

## 3. Approaches for full-editor GPU (to evaluate in the plan)

- **(A) Full Metal UI framework.** Re-implement every widget (analyzer, goniometer, phase-fan,
  meters, buttons, menus, labels, text, theming) as GPU geometry/shaders. Maximum performance &
  control; largest effort (months); essentially a custom immediate-mode GPU UI.
- **(B) Hybrid texture composite (recommended starting hypothesis).** One full-window
  `CAMetalLayer` owns the surface. The **analyzer** is drawn fresh every frame via Metal geometry
  (60 fps). The **rest of the UI** (meters/scopes/controls/background) is rendered by JUCE to an
  offscreen `juce::Image` at the message-thread rate (~13 fps in PT, fine for those), uploaded as
  a texture, composited by Metal each frame. Hard part: **input forwarding** — the Metal NSView
  covers the editor, so mouse/keyboard events must be routed back to JUCE's component tree, and
  JUCE's hit-testing/repaint coordinated. Much less than (A); intricate plumbing.
- **(C) WebView/WebGL full editor.** Render the whole UI in a `WKWebView` (JUCE 8
  `WebBrowserComponent`) with WebGL. Risk: WKWebView is also a layer-backed child — may hit the
  same PT coexistence wall (untested); heavy per-instance (a WebKit process each — bad for an
  analyzer users open many times); full rewrite in web tech. Lower priority.

Decision locked by user: **go Metal** (not WebView, not CPU). So evaluate **(A) vs (B)**; (B) is
the pragmatic first target if input forwarding proves tractable.

---

## 4. Architecture facts for the design

- **Render thread:** macOS `CVDisplayLink` (or `CADisplayLink`) per editor instance, driving a
  `CAMetalLayer`. No `MessageManager` lock, no `juce::Graphics`, no `Component::repaint`, no
  per-frame allocations on this thread.
- **Data in (lock-free):** the analyzer publishes a seqlock snapshot. Read via
  `AnalyzerEngine::getLatestSnapshot(...)` (`Source/analyzer/AnalyzerEngine.{h,cpp}`,
  `Source/dsp_adapters/AnalyzerSnapshotAdapter.h`, `Source/analyzer/AnalyzerSnapshot.h`). Safe to
  read from the render thread. **Never touch the audio thread / DSP.**
- **Per-frame pipeline to port to the render thread:** dB conversion, power-domain weighting,
  1/3-octave smoothing, ballistics, peak-hold. Currently runs message-thread-side in
  `AnalyzerDisplayView::analyzerUiTickCore()` + `mdsp::gui::AnalyzerRenderStateProvider`
  (`Source/ui/analyzer/AnalyzerDisplayView.cpp`). Move a copy to be **owned exclusively by the
  render thread**; message thread publishes an immutable `RenderConfig` (freq range, dB window,
  weighting mode, tilt, sample rate, plot rect) via `std::atomic<shared_ptr<const RenderConfig>>`
  or a seqlock. APVTS param reads are atomic and safe from the render thread.
- **Visual reference (must match):** the current CoreGraphics renderer lives in the HQ submodule
  `third_party/melechdsp-hq/shared/mdsp_ui` + `mdsp_gui`: `RTADisplay`, `RTADisplayRenderer`,
  `RTADisplayModel`, `BackgroundGridCache`, `RTADisplayInvalidationPolicy` (the last defines
  exactly when grid/labels must rebuild: viewMode/freq-range/dB-range/gain/theme/plot-rect — reuse
  those triggers for the cached grid texture). Goniometer/phase-fan/meters source code is
  app-side: `Source/ui/meters/*`, `Source/ui/analyzer/StereoScopeView.*`.
- **Where the renderer should live:** start **app-side** (`Source/ui/analyzer/metal/`) behind an
  `IAnalyzerSurfaceRenderer`-style interface so the CPU path remains the fallback; upstream into
  the HQ submodule only once stable (a submodule PR affects every Melech-DSP plugin).
- **Per-format gating is RUNTIME, not compile-time.** Use
  `audioProcessor.wrapperType == juce::AudioProcessor::wrapperType_AAX`. **Never** `#if
  JucePlugin_Build_AAX` — this is a single shared-code build where `Build_AAX/VST3/Standalone`
  are ALL `1`, so the macro is true in every format. (This bug already bit twice.) Note: Metal
  benefits all formats (lower CPU); recommend build format-agnostic, enable AAX-first.
- **CPU renderer stays as permanent fallback** behind a runtime capability check (Metal device/
  layer/display-link init failure ⇒ today's path).
- **Lifecycle:** stop the display link FIRST on teardown; pause/resume on window minimize/
  background/visibility change (Logic & PT assert/crash otherwise); handle display switch / GPU
  switch / drawable resize / retina scale.

---

## 5. Toolchain, build, sign, install, test (operational knowledge — important)

- **Env:** JUCE **8.0.12** at `/Users/avishaylidani/DEV/SDK/JUCE`. macOS, Apple Silicon. Repo
  root: `/Users/avishaylidani/DEV/GitHubRepo/MelechDSP/AnalyzerPro`. Build dir `build-debug/`.
  `juce_opengl` available; `juce_gui_extra` (NSViewComponent) available. JUCE has
  `juce_CGMetalLayerRenderer_mac.h` (its own Metal layer path; message-thread-driven — not our
  solution but reference).
- **Build:** `cmake --build build-debug --target AnalyzerPro_AAX` (also `_VST3`, `_Standalone`).
  Use `_Standalone` for fast compile checks (no sign needed). Strict warning flags are on
  (`-Wconversion -Wshorten-64-to-32 -Wzero-as-null-pointer-constant -Wpedantic` etc.) — keep
  builds warning-clean. Sources are listed explicitly in `target_sources(...)` in `CMakeLists.txt`
  (no glob) — add new files there. `.mm` for Objective-C++ (Metal/Cocoa). ARC is OFF (manual
  retain/release; CFRelease for CoreVideo objects).
- **PACE signing (required for retail Pro Tools):** `scripts/wraptool_sign_aax.sh <path-to-Mach-O>`.
  Creds in `scripts/.aax_wraptool.env` (WCGUID publisher route). **Requires the signing iLok
  plugged in (or an iLok Cloud session)** — otherwise `LICENSE ERROR: ... no attached iLok`.
  The human (avishay) manages the iLok; signing fails silently-ish without it.
- **Install gotcha:** the build auto-installs to
  `/Library/Application Support/Avid/Audio/Plug-Ins/AnalyzerPro.aaxplugin` but **ad-hoc signs**
  it (overwrites the PACE signature). So the flow is: build → `wraptool_sign_aax.sh` the artefact
  → `rm -rf "$DEST" && ditto <artefact> "$DEST"`. If the old bundle is root-owned, `mv` it aside
  (rm needs perms); a user-owned bundle deletes cleanly.
- **Pro Tools in-memory cache:** PT loads the plugin dylib once per session. After replacing the
  bundle you **must fully quit and relaunch Pro Tools** — reopening the plugin window does NOT
  reload it. Confirm the loaded build via the **bottom-left build stamp** (`AnalyzerPro vX • build
  <date> <time>`, already in `PluginEditor.cpp` via `__DATE__/__TIME__`).
- **Diagnostics pattern that works:** publish a value from the render thread into a
  `std::atomic<float>` in a tiny shared header, read it from the message-thread debug HUD
  (`AnalyzerDisplayView::accumulateDiagnosticsAndMaybeHud()`), which prints a line with paint/s,
  render fps, etc. Reuse this to show GPU-thread fps. Use a **cycling clear color** on the Metal
  surface as a visible frame-rate sanity check.
- **Cannot test in PT or sign from an automated agent** (needs the iLok + human). Plan for:
  agent implements + compiles clean; human signs/installs/relaunches PT and reports.

---

## 6. Current repo state (starting point)

- Branch `master` (pushed to origin) at commit **dcc3768** — the shipped CPU interim:
  - AAX idle fix (Timer tick, runtime `wrapperType`), AAX resize lock + 100/125/150 presets
    (persisted), the format-leak fix (VST3/AU/Standalone restored), uniform `Source/config/
    UiRates.h`, bottom-left build stamp.
  - All GPU experiment code REMOVED (the B0 Metal probe was a throwaway, reset out). Findings
    preserved in docs + memory only.
- The installed AAX plugin is this CPU build (PACE-signed).
- Playback is ~13 fps (host-capped); idle ~24 fps. That's the baseline to beat.

---

## 7. Existing docs & memory (read these)

- `docs/metal_renderer_plan.md` — the earlier phased plan. **CAVEAT: it assumed the
  Metal-analyzer + JUCE-rest HYBRID, which finding #6 INVALIDATED.** Useful for the rendering
  details (geometry, grid texture, threading) but the phasing must be rewritten for full-editor.
- `docs/metal_cursor_prompts.md` — phased B0–B5 Cursor prompts, **also written for the hybrid;
  supersede with full-editor phases.** Good for the operational rules/§0 CONTEXT block (reuse it).
- Project memory: `memory/project_rta_render_architecture.md` — the canonical record of all the
  findings above (PT starvation, GPU-thread-runs-free, child-Metal-dead-3-ways, where code lives).
- `memory/MEMORY.md` — index. Other memory: build paths, beta tasks, portal.

---

## 8. What the new session should produce (the plan)

A concrete plan for the full-editor Metal rewrite covering at least:

1. **Phase 0 — Metal hosting de-risk (full-editor):** confirm a full-window `CAMetalLayer` (the
   editor's own surface, NOT a child) composites at 60 fps in PT, multi-instance stable. This is
   the one unproven assumption. Decide: replace JUCE's editor NSView backing vs. a full-cover
   Metal NSView with input forwarding.
2. **Architecture decision: (A) full Metal widgets vs (B) Metal-composite-of-JUCE-texture +
   GPU analyzer + input forwarding.** Recommend prototyping (B); fall back to (A) per surface.
3. **Input/event forwarding** strategy (the hard part of B): route mouse/keyboard/trackpad from
   the Metal NSView to JUCE's component tree; keep hit-testing, hover, drag, menus working.
4. **Render-thread data pipeline** port (dB/weighting/smoothing/ballistics) + thread-safe config
   handoff; lock-free snapshot read.
5. **Analyzer GPU rendering**: spectrum line+fill (vertex buffers), glow, peak/hold, multi-trace,
   tilt, dB-window animation, hover crosshair/readout, grid+labels as cached texture (reuse the
   CoreGraphics grid image as a texture for exact parity to start). Visual-parity gate vs CPU.
6. **Scopes & meters**: GPU or JUCE-texture (per approach).
7. **Lifecycle/robustness**: minimize/background pause, display/GPU switch, resize/preset, retina,
   capability check + CPU fallback, multi-instance stress (8–16), leak checks.
8. **Per-format policy**, **submodule upstreaming**, **testing matrix**, **risks & fallbacks**,
   **effort estimate**, **decision points**.

Keep every phase shippable (flag off ⇒ current CPU UI). No DSP/audio changes. Runtime
`wrapperType`, never `#if JucePlugin_Build_AAX`. Don't edit `third_party/` until late.

---

## 9. First moves for the new session

1. Read this doc + `memory/project_rta_render_architecture.md` + `docs/metal_renderer_plan.md`.
2. Skim the current renderer for visual parity: HQ `mdsp_ui/.../RTADisplayRenderer`, and
   `Source/ui/analyzer/AnalyzerDisplayView.cpp` (pipeline) + `Source/ui/meters/*` (scopes/meters).
3. Resolve the §8.1 de-risk question (full-editor Metal compositing in PT) and the (A)-vs-(B)
   architecture choice — those gate everything.
4. Produce the phased plan. Then implement Phase 0 (full-editor Metal hosting probe), build,
   hand a signed build to the human for the PT 60 fps + multi-instance check.
