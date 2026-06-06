# Platform Architecture — Standing Principle

**One codebase. macOS GPU rendering is a presentation layer, not a fork.**

AnalyzerPro (and future Melech-DSP plugins) ship from a **single shared source tree** that
compiles for both macOS and Windows. The "Apple Metal" GPU editor is a **macOS-only presentation
layer**, isolated behind compile-time guards. It is *not* a separate "Mac plugin" codebase.

## The rules

1. **Features live in shared, platform-neutral core.** DSP, the analyzer/`AnalyzerEngine`,
   parameters, trace definitions, analyzer/UI logic — all platform-agnostic. Both platforms get
   every feature automatically.

2. **The GPU path is a "dumb renderer" of shared engine state.** It reads the lock-free
   `AnalyzerEngine` snapshot and *re-renders* it at display rate. It MUST NEVER compute a feature
   the shared engine doesn't already produce. The data is identical on Mac and Windows; only the
   *rendering* differs. (Render-thread ballistics/smoothing of shared snapshot data is fine — that's
   rendering, not a feature.)

3. **`IEditorSurface` selects the renderer at runtime** — Metal on macOS (when enabled), CPU
   everywhere else. New UI goes into the CPU editor / shared widgets; the Metal path composites the
   same JUCE component tree as a texture, so it inherits UI features for free.

4. **The CPU editor is always intact and is the universal fallback.** Flag off, non-macOS, or any
   Metal init failure ⇒ today's CoreGraphics editor, unchanged.

## What legitimately lives in the macOS-only path

Only the GPU rendering itself: shaders, the `CAMetalLayer`/render-thread present, BackingLayer
hosting, chrome capture, and the render-thread pipeline that re-renders shared snapshot data at
display rate. Everything else is shared. **If feature logic leaks into `MetalHost.mm`, Mac and
Windows diverge — that is the anti-pattern to police in review.**

## Compile/runtime gating

- **Compile-time:** all Metal code is behind the CMake flag `ANALYZERPRO_METAL_EDITOR` (macOS only).
  On Windows the flag is off and none of it compiles → Windows is provably unaffected.
- **Runtime format gate:** Metal currently starts only for **AAX** (`wrapperType ==
  wrapperType_AAX`) — a deliberate scope choice (Pro Tools' ~13 fps message-thread cap is the
  problem it solves). VST3/AU/Standalone on macOS use the CPU editor. Extending Metal to other
  macOS formats is a separate per-host validation phase (Logic/AU, Live/VST3 have their own
  window/visibility lifecycles) — do NOT widen the gate without validating each host.
- **Per-format behavior is ALWAYS a runtime `wrapperType` check, never `#if JucePlugin_Build_AAX`**
  (that macro is true in every format in this single-compilation build — it has bitten twice).

## When adding a feature that applies to both platforms

Implement it **once in shared code**. Windows renders it via CPU; macOS renders it via CPU *or*
gets the fast Metal version automatically *if* the renderer just reads the new shared state. Never
duplicate feature logic into the Metal layer.

See `GPU_PLUGIN_RENDERING_PLAYBOOK.md` for the reusable GPU-rendering implementation guide.
