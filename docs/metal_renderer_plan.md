# AnalyzerPro — Metal Renderer Plan (Phase B)

Status: **green-lit, de-risked.** A blank-screen probe measured the GL/CVDisplayLink
render thread holding **~57–60 fps during Pro Tools playback** while the message-thread
analyzer was pinned at ~13 fps. The GPU render thread is a system display service PT
cannot starve. This document is the implementation plan for the bespoke renderer that
exploits that.

---

## 1. Goal & success criteria

- Analyzer spectrum (and ideally scopes/meters) render at **display rate (~60 fps) during
  Pro Tools playback**, not the ~13 fps the message thread is capped to.
- **Visual parity** with the current CoreGraphics renderer — same traces, fills, glow,
  grid, labels, colors, typography. Not a downgrade.
- **No DSP / audio-thread changes.** Render thread only consumes the existing lock-free
  snapshot.
- **Multi-instance stable** in Pro Tools (the historical failure mode for GPU plugins).
- **Graceful fallback** to the CPU renderer if GPU init fails or on unsupported setups.

## 2. Why CPU/JUCE-GL can't do this (recap, so we don't relitigate)

- Pro Tools grants the **MessageManager lock only ~13×/sec during playback**. Anything that
  paints through the JUCE component tree (CPU *or* `OpenGLContext` component-painting) needs
  that lock → capped at ~13 fps.
- JUCE's `OpenGLContext` rasterizes vector paths on the CPU via tessellation → 22–50 ms/frame
  for our content (vs 3.7 ms CoreGraphics). So "just turn on GL" is both gated *and* slow.
- The escape is a renderer that (a) runs on the **CVDisplayLink thread** (no MM lock) and
  (b) draws with **efficient GPU geometry** (vertex buffers + shaders), not `juce::Graphics`.

## 3. Architecture

```
audio thread ──► PublishedAnalyzerSnapshot (seqlock, lock-free)   [EXISTS]
                          │  (read by render thread, no locks)
                          ▼
CVDisplayLink render thread (per editor instance):
   1. read latest snapshot (lock-free)
   2. run dB / weighting / octave-smoothing / ballistics / peak-hold
        (port of analyzerUiTickCore + RenderStateProvider, thread-confined to render thread)
   3. build vertex data for traces (line strip + fill triangles)
   4. encode Metal command buffer:
        - blit cached GRID/LABEL texture (rebuilt only on geometry/theme/range change)
        - draw trace fills (triangle strip, alpha gradient via fragment shader)
        - draw trace lines (anti-aliased, glow via shader or 2-pass)
        - draw peak/hold + multi-traces
   5. present CAMetalLayer drawable
```

- **One `CAMetalLayer` per editor**, hosted in the analyzer area of the JUCE editor.
- **Own `CVDisplayLink`** (or `CADisplayLink`) per instance driving the render thread.
  (This is what the probe proved runs free in PT.)
- Message thread still owns: param changes, mouse/hover, layout, theme — it pushes immutable
  config snapshots to the render thread (double-buffered / atomic), never shares mutable state.

## 4. Threading & data handoff (the careful part)

- **Snapshot in:** already lock-free (`AnalyzerEngine::getLatestSnapshot`, seqlock). Render
  thread reads directly. ✔
- **Pipeline state** (smoothing history, ballistics, peak-hold decay) currently lives in
  `AnalyzerRenderStateProvider` and runs on the message thread (`analyzerUiTickCore`). Move a
  copy to be **owned exclusively by the render thread** — no other thread touches it. APVTS
  param reads (range, weighting, tilt, dB window) are atomic and safe to read from the render
  thread; cache them into an immutable `RenderConfig` the message thread publishes via
  `std::atomic<shared_ptr>` or a seqlocked struct.
- **Geometry / plot rect / hover / crosshair:** message thread computes, publishes immutable
  snapshot to render thread. Hover crosshair can be a separate cheap overlay.
- **No `juce::Graphics`, no component repaint, no MM lock on the render path.**

## 5. Rendering details

- **Grid + axis labels** → render once to an offscreen texture (Metal render-to-texture, or
  even reuse the existing CoreGraphics `BackgroundGridCache` image uploaded as a texture).
  Rebuild only on size/theme/freq-range/dB-range change (the existing
  `RTADisplayInvalidationPolicy` already tracks exactly these triggers — reuse its logic).
- **Spectrum trace** → vertex buffer of (x,y) per bin; line via triangle-expanded strip for
  width + AA, fill via triangle strip to baseline with an alpha-gradient fragment shader.
- **Glow** → either a second blurred pass or an SDF-style fragment shader; tune to match the
  current look.
- **Multi-trace / peak / hold** → same pipeline, additional vertex buffers + blend.
- **Text** (labels, readouts): cached in the grid texture where static; dynamic readouts
  (hover Hz/dB) can be a small CoreGraphics-rendered texture updated on change, or a glyph
  atlas. Keep dynamic text off the per-frame hot path.

## 6. Where the code lives

- The analyzer renderer (`RTADisplay`, `RTADisplayRenderer`, model, cache, invalidation) lives
  in the **HQ submodule** `third_party/melechdsp-hq/shared/mdsp_ui` + `mdsp_gui`. The Metal
  renderer is analyzer-specific and benefits every Melech-DSP plugin → it **belongs in the
  submodule** long-term.
- **But** start it as a **parallel renderer** behind an interface so the CPU path stays intact:
  `IAnalyzerSurfaceRenderer { CPU impl (today) | Metal impl (new) }`. Editor picks at runtime.
- Hosting glue (CAMetalLayer + CVDisplayLink + NSView embed) can begin app-side
  (`Source/ui/analyzer/metal/`) and upstream into the submodule once proven.

## 7. Phasing (de-risk first, always shippable)

- **B0 — Metal hosting de-risk (small).** Self-host a `CAMetalLayer` in the editor + own
  `CVDisplayLink`, clear to a cycling color + fps readout (the Metal twin of the GL probe we
  built). **Confirm ~60 fps in PT during playback AND multi-instance (8+) stability + clean
  teardown.** Gate: if a self-hosted Metal layer behaves worse than `juce::OpenGLContext` did,
  fall back to driving the thread via `OpenGLContext` + raw GL geometry instead. This phase
  answers "does our own Metal hosting work in PT" before any real rendering.
- **B1 — Spectrum MVP.** FFT trace (line + fill) in Metal, reading the seqlock, grid as a
  cached texture (reuse `BackgroundGridCache` image as a texture). Render-thread pipeline port.
  Behind a flag, AAX-first. Compare side-by-side with CPU for parity.
- **B2 — Visual parity.** Glow, peak/hold, multi-trace (L/R/M/S/Mono/Stereo), tilt, dB window
  animation, hover crosshair + readout, theme colors, axis labels. Pixel-compare vs CPU.
- **B3 — Lifecycle & robustness.** Pause/resume on window minimize/background (Logic/PT
  assertions are a known Metal pitfall), display change / GPU switch handling, context-loss
  recovery, fallback-to-CPU on init failure, retina/scale changes, resize/preset changes.
- **B4 — Scopes & meters (optional).** Port goniometer + phase-fan + meters to GPU, or leave
  them CPU (they're secondary surfaces). Decide based on B1–B3 cost.
- **B5 — Upstream into the submodule**, behind the renderer interface, enable per-format.

Each phase leaves the plugin shippable (flag off ⇒ current CPU renderer).

## 8. Risks & mitigations

| Risk | Mitigation |
|---|---|
| Self-hosted Metal layer misbehaves in PT's hosted view | B0 proves it before investing; fallback = `OpenGLContext`-driven raw GL (proven to attach at top level) |
| Multi-instance GPU pressure / crashes | Stress 8–16 instances in B0; share Metal device; cap layer count; fallback to CPU |
| Background/minimize crashes (Logic/PT) | Explicit CVDisplayLink stop + layer pause on peer visibility change (B3) |
| Visual drift from CoreGraphics look | Side-by-side parity gate each phase; reuse grid cache image as texture for exact match |
| OpenGL deprecation (if we fall back to GL) | Metal is primary; GL only as interim fallback |
| Render-thread data races | Strict ownership: render thread owns pipeline state; message thread publishes immutable config via atomics/seqlock |
| Scope creep into shared module | Build app-side behind an interface first; upstream only once stable |

## 9. Fallback strategy

- Runtime capability check at editor construction. If Metal device / layer / display link
  init fails → use the existing CPU renderer (today's path). Never hard-depend on GPU.
- Keep the CPU renderer as the permanent baseline; Metal is an acceleration layer.

## 10. Testing / verification

- **FPS gate:** render-thread fps HUD (reuse the probe's atomic→HUD plumbing) must hold ~60
  during PT playback, multi-instance.
- **Parity gate:** screenshot diff vs CPU renderer per phase.
- **Stability gate:** 8–16 instances, open/close churn, minimize/restore, sample-rate change,
  display switch — no crashes, no leaks (Metal frame capture / leaks).
- **No-DSP-change gate:** audio path untouched; render thread is read-only on the snapshot.

## 11. Open decisions (need a call before B1)

1. **Metal-native vs OpenGL-thread + raw GL** for the first real renderer. Recommend Metal
   (future-proof); B0 decides if self-hosting is clean enough, else GL interim.
2. **AAX-only or all formats.** VST3/Standalone already get smooth VBlank; Metal still lowers
   CPU and unifies. Recommend: build format-agnostic, enable AAX-first.
3. **Scopes/meters on GPU now or later** (B4 optional).
4. **App-side first vs straight into the HQ submodule.** Recommend app-side behind an interface,
   upstream after B3.

## 12. Immediate next step

Build **B0** — the Metal hosting probe (CAMetalLayer + own CVDisplayLink, cycling color + fps).
~1 short iteration; it confirms self-hosted Metal renders at 60 fps in PT and survives
multi-instance before we port any real drawing. This is the last unknown.
