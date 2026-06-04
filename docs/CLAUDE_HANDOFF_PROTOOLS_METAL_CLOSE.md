# Claude Handoff: Pro Tools Metal Close Fix

## Context

AnalyzerPro has an experimental full-editor Metal UI path behind `ANALYZERPRO_METAL_EDITOR`, enabled at runtime for AAX. The recent work added a `CAMetalLayer`-backed editor surface driven by `CADisplayLink` from `Source/ui/analyzer/metal/MetalHost.mm`.

After the first Metal renderer checkpoint, Pro Tools would become stale/hung when closing the plugin window. Opening/rendering worked, but closing the editor blocked PT.

## What We Confirmed

- The issue was in Metal display-link teardown, not DSP or analyzer data.
- `CADisplayLink.invalidate()` and render-thread shutdown were still happening too synchronously from the Pro Tools close path.
- `CAMetalLayer::nextDrawable()` / present completion can stall during host teardown if the close thread waits on the render thread.
- A bounded join alone did not fix it.

## Fix That Worked

File changed:

- `Source/ui/analyzer/metal/MetalHost.mm`

Working fix:

- Set `stopping` before teardown so `renderFrame()` exits early.
- If teardown begins after `nextDrawable()` returns, bail before encoding/presenting.
- Do not call `CADisplayLink invalidate` directly from the Pro Tools close thread when a render run loop exists.
- Retain the display link, schedule invalidation on the render run loop via `CFRunLoopPerformBlock`, wake the run loop, and stop it from there.
- Wait only briefly for render-thread exit.
- If the render thread still does not exit, abandon the Metal host context instead of blocking Pro Tools or freeing memory reachable by the callback.

## Validation Performed

Built successfully with Metal enabled:

```bash
cmake --build build-debug --target AnalyzerPro_Standalone
cmake --build build-debug --target AnalyzerPro_AAX
```

PACE signed and installed the Debug AAX:

```bash
./scripts/wraptool_sign_aax.sh \
  "build-debug/AnalyzerPro_artefacts/Debug/AAX/AnalyzerPro.aaxplugin/Contents/MacOS/AnalyzerPro"

ditto \
  "build-debug/AnalyzerPro_artefacts/Debug/AAX/AnalyzerPro.aaxplugin" \
  "/Library/Application Support/Avid/Audio/Plug-Ins/AnalyzerPro.aaxplugin"
```

Verified in place with `wraptool verify`.

Human Pro Tools result:

- Plugin window now closes successfully.
- Pro Tools no longer becomes stale/hung on close.

## Git State

A checkpoint commit was created before the teardown fix:

- `d4b0850 ui: checkpoint full-editor Metal renderer`

The working fix is currently an uncommitted change in:

- `Source/ui/analyzer/metal/MetalHost.mm`

Recommended next action:

- Review the `MetalHost.mm` diff.
- Commit as a focused lifecycle fix, e.g. `fix: avoid Pro Tools hang on Metal editor close`.
