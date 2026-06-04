# AnalyzerPro — Full-Editor Metal UI Rewrite Plan

Status: **planning complete, unimplemented.** Supersedes the phasing in
`docs/metal_renderer_plan.md` (B0–B5), whose hybrid "Metal analyzer + JUCE rest"
assumption was **invalidated** by the B0 finding (a child Metal view blanks the whole
JUCE editor in Pro Tools). The rendering details in that doc still stand; only the
phasing/coexistence model is rewritten here.

Read first: `docs/metal_ui_rewrite_HANDOFF.md` (the self-contained brief) and
`memory/project_rta_render_architecture.md` (canonical findings). This plan assumes both.

---

## 0. The decisions, locked

| Decision | Call | Why |
|---|---|---|
| GPU API | **Metal** (not WebView, not GL) | User-locked. GL deprecated on mac; WKWebView is a layer-backed child → likely hits the same coexistence wall, plus a WebKit process per instance. |
| UI strategy | **(B) Metal composite of a JUCE texture + GPU analyzer + input forwarding** as primary; **(A) native Metal widgets** only per-surface if B's parity/latency is unacceptable | B is weeks, A is months. The whole UI need not be 60 fps — only the analyzer (and arguably the scopes) must be. Everything else is fine at the ~13 fps message-thread rate as long as **input stays responsive** (input is event-driven, not frame-rate-bound). |
| Per-format gating | **Runtime `audioProcessor.wrapperType == wrapperType_AAX`**, build format-agnostic, enable AAX-first | Single shared-code build: `JucePlugin_Build_AAX/VST3/Standalone` are ALL `1`. `#if JucePlugin_Build_AAX` is true in every format — this bug bit twice. Metal lowers CPU for all formats; ship it everywhere once stable. |
| Code location | **App-side** `Source/ui/analyzer/metal/` behind an `IEditorSurface`/`IAnalyzerSurfaceRenderer` interface; upstream to the HQ submodule only after Phase 4 | A submodule change touches every Melech-DSP plugin. Don't pay that until proven. |
| Fallback | **CPU renderer stays permanent**, selected at runtime on any Metal init failure | Never hard-depend on GPU. |

Non-negotiables carried through every phase: **no DSP / audio-thread changes** (render
thread is read-only on the lock-free snapshot); **every phase shippable** (flag off ⇒
today's CPU editor, byte-for-byte); **no `juce::Graphics` / no `MessageManager` lock / no
allocations on the render thread**.

---

## 1. The one unproven assumption (gates everything)

Full-screen **GL** owning the whole editor surface composited correctly at ~60 fps in PT
(blank probe). Full-screen **Metal** specifically was never reached — the child-Metal probe
blanked the editor before we got there. So Phase 0 exists to answer exactly one question:

> **Does a full-editor `CAMetalLayer` (the editor's own surface, not a child overlay)
> composite in Pro Tools, at ~60 fps, multi-instance, with clean teardown?**

High confidence (GL did), but unproven. If Metal-full-surface also fails in PT, the
fallback is a **full-surface `OpenGLContext`-driven raw-GL renderer** (proven to attach at
top level) — same architecture below, GL backend instead of Metal. Keep the renderer
backend behind the interface so this swap is localized.

### 1a. Two hosting mechanisms to decide in Phase 0

Both put a single Metal/CAMetalLayer surface over the *entire* editor; they differ in how
JUCE's NSView relates to it:

- **Mechanism 1 — replace the editor peer's backing layer.** Make JUCE's top-level editor
  NSView layer-backed by *our* `CAMetalLayer` (via `getPeer()->getNativeHandle()`,
  `wantsLayer=YES`, swap `view.layer`). JUCE keeps its component tree, hit-testing, and
  event routing intact; we just take over what gets presented. **Risk:** JUCE may fight us
  for the layer / repaint into it; B0 showed `JUCE_COREGRAPHICS_RENDER_WITH_MULTIPLE_PAINT_CALLS=1`
  (JUCE's own Metal backing) still blanked — so JUCE-owned Metal backing is suspect, but a
  *we-own-it* backing where JUCE never paints to the screen layer may differ.
- **Mechanism 2 — full-cover Metal `NSView` + input forwarding.** Our own `NSView` hosting
  the `CAMetalLayer` is added as the topmost subview covering the editor; JUCE renders
  offscreen (to a texture); the cover view forwards all mouse/keyboard/trackpad events back
  into JUCE's component tree manually (see §3). **Risk:** input forwarding is the hard part;
  this is the path that needs §3 plumbing. **But** it's the path the GL probe most resembles
  (top-level context, component painting disabled).

**Phase 0 builds the smallest version of each and picks the one that composites in PT.**
Recommendation if both work: Mechanism 1 (keeps JUCE event routing free → §3 mostly
evaporates). Plan for Mechanism 2 since it's the one the probe validated.

---

## 2. Target architecture (Approach B)

```
audio thread ─► PublishedAnalyzerSnapshot (seqlock, lock-free)         [EXISTS, untouched]
                       │ read lock-free
                       ▼
┌─ message thread (PT-capped ~13 fps) ─────────────────────────────────────────────┐
│ JUCE component tree: HeaderBar, ControlRail(Viewport), FooterBar, meters,         │
│   scopes, loudness panel, tooltips, menus, build stamp.                           │
│ Renders to an OFFSCREEN juce::Image ("chrome texture") on change/at ~13 fps.      │
│ Publishes, via std::atomic<shared_ptr<const RenderConfig>> (or seqlock):          │
│   - chrome texture (when dirty)                                                    │
│   - RenderConfig: freq range, dB window, weighting mode, tilt, sample rate,       │
│     plot rect, theme colors, trace enable/colors, hover x/state                   │
│ Receives forwarded input events (Mechanism 2) and routes to components.           │
└───────────────────────────────────────────────────────────────────────────────────┘
                       │ (immutable handoff only)
                       ▼
┌─ CVDisplayLink render thread, per editor instance (60 fps, no MM lock) ──────────┐
│ 1. read latest analyzer snapshot (lock-free)                                      │
│ 2. run render-thread-owned pipeline: dB → power weighting → 1/3-oct smoothing →   │
│    ballistics → peak/hold   (port of analyzerUiTickCore + RenderStateProvider)    │
│ 3. build trace vertex buffers (line strip expanded for width+AA; fill to baseline)│
│ 4. encode Metal command buffer:                                                   │
│      a. draw composited CHROME texture (full-window quad)  ── the JUCE UI         │
│      b. draw cached GRID/LABEL texture over the plot rect                         │
│      c. draw analyzer fills → lines → glow → peak/hold → multi-trace              │
│      d. draw hover crosshair/readout overlay                                      │
│ 5. present CAMetalLayer drawable                                                  │
└───────────────────────────────────────────────────────────────────────────────────┘
```

Key property: the chrome texture going stale to ~13 fps is invisible — header/meters/
controls don't need 60 fps. Only the analyzer layer (and optionally scopes) is redrawn
fresh every frame from live data. Input is event-driven, so it stays crisp regardless of
the chrome's redraw cadence.

### 2a. The editor widget inventory (what "chrome" must cover)

From `Source/ui/MainView.h`: `HeaderBar`, `ControlRail` inside a `juce::Viewport` (with
`ComponentAnimator` width animation), `FooterBar`, `AnalyzerDisplayView` (→ HQ
`RTADisplay`), `StereoScopeComponent` (goniometer), `PhaseFanScopeComponent`,
`LoudnessNumericPanel`, two `MeterGroupComponent` (in/out), `TooltipManager` overlay,
`TooltipWindow`, build-stamp label. Plus combos/sliders/buttons throughout and APVTS
attachments. All of this stays as-is and renders to the chrome texture; the analyzer's
spectrum is the one surface punched out and drawn on the GPU.

---

## 3. Input/event forwarding (the hard part of B, Mechanism 2 only)

If Phase 0 picks Mechanism 1, **skip most of this** — JUCE keeps native event routing.
For Mechanism 2 (cover NSView), forward from the Metal NSView into JUCE:

- Override `NSView` mouse/key/scroll/gesture handlers on the cover view; translate each to
  the JUCE coordinate space and dispatch into the editor's component tree via
  `Component::getComponentAt()` + synthesized `juce::MouseEvent`/`handleMouseEvent`, or by
  forwarding to the JUCE peer's `handleMouseEvent`/`handleKeyPress` on the message thread.
- Preserve: hover/enter/exit transitions, drag (mouseDown→drag→up capture), right-click,
  double-click, modifier keys, mouse-wheel/trackpad on the analyzer and rail viewport,
  keyboard focus + text entry (if any), and **menus/popups** (combo dropdowns,
  context menus) — these open their own native windows; verify they composite over the
  Metal surface (they're separate `NSWindow`s, should be fine, but **test in PT**).
- Tooltips: the custom `TooltipManager` overlay paints into the chrome texture; the JUCE
  `TooltipWindow` is a separate window — verify it shows over Metal.
- Hit-testing parity: the analyzer's hover crosshair/readout is interactive — route hover x
  to `RenderConfig.hover` so the GPU draws the crosshair, while the value-readout text comes
  from the message-thread side (cheap, low rate is fine).

This is intricate but bounded. De-risk it in Phase 0b with a single clickable button under
the Metal surface before committing.

---

## 4. Render-thread data pipeline port

Currently message-thread-side in `AnalyzerDisplayView::analyzerUiTickCore()` +
`mdsp::gui::AnalyzerRenderStateProvider`. Port a **copy owned exclusively by the render
thread** (no sharing):

- Stages: dB conversion, **power-domain** weighting (per MEMORY: applied before octave
  smoothing — keep that order), 1/3-octave smoothing, ballistics, peak/hold decay. Internal
  dB floor −200; `sanitizeDb` clamp [−200, 24] (match the CPU path exactly).
- Inputs: lock-free snapshot (`AnalyzerEngine::getLatestSnapshot`, see
  `Source/analyzer/AnalyzerSnapshot.h`, `Source/dsp_adapters/AnalyzerSnapshotAdapter.h`) +
  immutable `RenderConfig` published by the message thread. APVTS param reads are atomic and
  safe from the render thread, but prefer caching them into `RenderConfig` for a consistent
  per-frame view.
- Smoothing/ballistics history is **render-thread-private mutable state**; nothing else
  touches it. This removes the "double ballistics" coupling concern — there's one owner.
- Parity gate: drive the same snapshot through CPU and render-thread pipelines offline and
  assert bin-for-bin agreement (±epsilon) before trusting the GPU draw.

---

## 5. Analyzer GPU rendering (visual parity is a hard gate)

Match the CoreGraphics renderer in the HQ submodule (`mdsp_ui`/`mdsp_gui`:
`RTADisplayRenderer`, `RTADisplayModel`, `BackgroundGridCache`,
`RTADisplayInvalidationPolicy`).

- **Grid + axis labels:** render once to an offscreen texture and blit each frame. To start,
  **reuse the existing `BackgroundGridCache` CoreGraphics image uploaded as a Metal texture**
  for exact parity. Rebuild only on the triggers `RTADisplayInvalidationPolicy` already
  defines: viewMode / freq-range / dB-range / gain / theme / plot-rect. Reuse that policy
  verbatim.
- **Spectrum trace:** vertex buffer of (x,y) per bin; line as a triangle-expanded strip for
  width + AA; fill as a triangle strip to the baseline with an alpha-gradient fragment
  shader.
- **Glow:** second blurred pass or SDF fragment shader; tune to the current look.
- **Multi-trace / peak / hold:** same pipeline, extra vertex buffers + blend (L/R/M/S/Mono/
  Stereo; user trace colors come from `RenderConfig`, sourced from `TraceColorStore`).
- **Tilt, dB-window animation:** parameters in `RenderConfig`; animation interpolation runs
  render-thread-side.
- **Hover crosshair/readout:** crosshair drawn on GPU from `RenderConfig.hover`; the
  Hz/dB text readout is rendered message-thread-side into a small texture (off the hot path).
- **Parity gate per phase:** screenshot-diff GPU vs CPU at identical config; no visible drift.

---

## 6. Scopes & meters

Decide after Phase 3 based on measured cost:
- **Default:** leave goniometer (`StereoScopeComponent`), phase-fan
  (`PhaseFanScopeComponent`), and meters (`MeterGroupComponent`) in the **chrome texture**
  (message-thread rate). They're secondary; ~13 fps during playback is acceptable, and they
  ship "for free" via the composite.
- **Upgrade path (optional):** if scopes feel laggy in playback, port the goniometer/phase-fan
  to GPU geometry (they already have `StereoScopeRenderStateProvider` /
  `PhaseFanRenderStateProvider` lock-free providers in `mdsp_ui/scopes`, so the data handoff
  pattern matches the analyzer's). Meters are cheap bars — likely never worth GPU.

---

## 7. Lifecycle & robustness

- **Teardown order:** stop the `CVDisplayLink` **first**, then release drawable/layer/device
  (Logic & PT assert/crash otherwise). Mirror in `~Editor`.
- **Pause/resume** on window minimize / app background / peer visibility change (known Metal
  pitfall in hosts).
- Handle **display switch / GPU switch**, **drawable resize**, **retina/backing-scale change**,
  and the AAX **size presets (100/125/150)** + resize-lock already in `PluginEditor.cpp`.
- **Capability check** at editor construction: Metal device + layer + display-link init; any
  failure ⇒ today's CPU path. Cycling clear-color + fps HUD as a liveness check (reuse the
  atomic→HUD plumbing in `accumulateDiagnosticsAndMaybeHud()`).
- **Multi-instance:** share one `MTLDevice`; stress 8–16 instances + open/close churn; watch
  for leaks (Metal frame capture / Instruments).

---

## 8. Phasing (each phase shippable; flag off ⇒ current CPU editor)

Flag: `ANALYZERPRO_METAL_EDITOR` (runtime-gated to AAX first via `wrapperType`).

- **Phase 0 — Full-editor Metal hosting de-risk.** *(THE gate.)* Build minimal Mechanism 1
  and Mechanism 2; clear to a cycling color + render-thread fps HUD, no real drawing.
  **Exit criteria:** a full-editor `CAMetalLayer` holds ~60 fps during PT playback,
  8+ instances stable, clean teardown. Pick the mechanism. **0b:** one forwarded clickable
  button (validates §3 if Mechanism 2). *If both Metal mechanisms blank in PT → switch backend
  to full-surface `OpenGLContext` raw-GL and re-run Phase 0.*
- **Phase 1 — Chrome composite + analyzer MVP.** JUCE editor → offscreen `juce::Image` →
  Metal texture → full-window quad (the whole current UI, composited by Metal). Then punch in
  the analyzer: FFT trace (line + fill) drawn from the lock-free snapshot via the
  render-thread pipeline; grid as the reused `BackgroundGridCache` texture. Side-by-side
  parity vs CPU. **Exit:** UI looks identical, analyzer animates at 60 fps in PT playback,
  input works.
- **Phase 2 — Analyzer visual parity.** Glow, peak/hold, multi-trace, tilt, dB-window
  animation, hover crosshair + readout, theme colors, user trace colors, axis labels.
  Screenshot-diff gate vs CPU.
- **Phase 3 — Lifecycle & robustness.** §7 in full: pause/resume, display/GPU switch,
  resize/preset, retina, capability check + CPU fallback, multi-instance stress, leak checks.
- **Phase 4 — Scopes/meters decision** (§6) + **per-format enablement** (turn on VST3/AU/
  Standalone) + **submodule upstreaming** behind the renderer interface.

Operational note (from handoff §5): an automated agent **cannot sign or test in PT** (needs
the iLok + human). Each phase: agent implements + compiles warning-clean
(`cmake --build build-debug --target AnalyzerPro_Standalone` for fast checks; `_AAX` for the
real artifact) → human runs `scripts/wraptool_sign_aax.sh` → `rm -rf $DEST && ditto` →
**fully quit & relaunch PT** → confirms via bottom-left build stamp + fps HUD.

---

## 9. Risks & fallbacks

| Risk | Mitigation / fallback |
|---|---|
| Full-editor Metal also blanks in PT | Phase 0 proves before investment; fallback = full-surface GL backend (probe-proven), same architecture |
| Input forwarding (Mechanism 2) too lossy | Prefer Mechanism 1 (keeps JUCE routing); de-risk in Phase 0b before building on it |
| Menus/tooltips don't composite over Metal | Test explicitly in Phase 0/1 (separate NSWindows — expected OK, unverified in PT) |
| Visual drift from CoreGraphics | Reuse grid-cache image as texture; per-phase screenshot-diff gate |
| Render-thread data races | Strict ownership: render thread owns pipeline state; message thread publishes immutable config via atomic<shared_ptr>/seqlock |
| Multi-instance GPU pressure | Share MTLDevice; stress 8–16; CPU fallback per instance on init failure |
| Background/minimize crash | Explicit display-link stop + pause on visibility change (§7) |
| Scope creep into HQ submodule | App-side behind interface; upstream only after Phase 3 |

---

## 10. Effort & decision points

- **Effort (rough):** Phase 0 ≈ 1–2 iterations (it's a probe). Phases 1–3 ≈ several weeks of
  focused work (composite plumbing + pipeline port + parity + lifecycle), dominated by
  parity-chasing and PT round-trips (human-in-the-loop sign/test latency). Phase 4 is
  incremental. Approach A (native widgets) would be months — only fall back to it per-surface
  if B's parity/input is unacceptable.
- **Decisions that must be made empirically, in order:** (1) Phase 0 — does full-editor Metal
  composite in PT, and which hosting mechanism. (2) After Phase 1 — is chrome-at-13fps +
  analyzer-at-60fps the right feel, or do scopes also need GPU. (3) After Phase 3 — enable
  other formats / upstream now or hold.

## 11. First moves for the implementing session

1. Re-read this plan + handoff + `project_rta_render_architecture.md`.
2. Add `Source/ui/analyzer/metal/` + the `ANALYZERPRO_METAL_EDITOR` flag (default OFF) +
   the renderer interface; wire nothing into the live path yet.
3. Implement **Phase 0** (both hosting mechanisms, cycling color + fps HUD), compile clean,
   hand a signed AAX build to the human for the PT 60 fps + multi-instance check.
4. On a green Phase 0, proceed to Phase 1.
