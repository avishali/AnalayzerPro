# Claude Handoff: Metal Reopen Stale UI

## Current Symptom

In Pro Tools AAX with `ANALYZERPRO_METAL_EDITOR=ON`:

- First open can show the trace correctly.
- Closing the plugin window no longer hangs Pro Tools.
- Reopening the plugin window can leave the analyzer trace gone.
- When this happens, static/dynamic chrome content such as meters also stops refreshing.

Because meters are part of the full-editor chrome texture path, this is probably not just an analyzer trace rendering bug. It points to the Metal host/chrome capture/display-link lifecycle after close/reopen.

## Recent Good State

Committed fixes:

- `d4b0850 ui: checkpoint full-editor Metal renderer`
- `9540580 fix: avoid Pro Tools hang on Metal editor close`

The close hang was solved by moving `CADisplayLink` invalidation onto the render run loop and by avoiding an unbounded join on Pro Tools' close thread.

## Current Uncommitted State

There are uncommitted parity/fallback edits in:

- `Source/ui/analyzer/AnalyzerDisplayView.cpp`
- `Source/ui/analyzer/AnalyzerDisplayView.h`
- `Source/ui/analyzer/metal/MetalHost.mm`
- `Source/ui/analyzer/metal/MetalHostShared.h`

These edits attempt to:

- Extend `MetalAnalyzerFrame` with trace style payloads.
- Send trace colors/toggles from `AnalyzerDisplayView`.
- Draw live trace data in `MetalHost.mm` using `AnalyzerEngine::getLatestSnapshot()`.
- Fail open to a default live RMS trace if no configured trace payload draws.

These changes build/sign/install, but the user still reports intermittent no-refresh after close/reopen.

## Most Likely Cause

The strongest suspect is the close-fix escape hatch in `MetalHost::stop()`:

- If `MetalHostImpl::stop()` cannot stop the render thread within the short timeout, `MetalHost::stop()` calls `impl_.release()`.
- That intentionally leaks/abandons `MetalHostImpl` to avoid blocking Pro Tools close.
- When that happens, cleanup after `stopDisplayLinkAndDrain()` is skipped:
  - `detachFromPeerView()` may not run.
  - The backing `CAMetalLayer` may not be restored/detached.
  - `displayLink`, `displayLinkTarget`, `metalLayer`, command queue, and peer/layer references may remain alive.
  - The leaked object still has stale native-view/layer state from the closed editor.

This matches the observed behavior: Pro Tools does not hang, but the next editor open can be covered by or compete with a stale Metal surface/context, so the chrome texture and analyzer overlay stop updating.

Confirm this by checking Console logs around close/reopen for:

```text
[MetalHost #N] abandoned teardown after timeout (...)
[MetalHost #N] abandoned teardown from render thread (...)
```

If one of those appears before the bad reopen, this is the root cause.

## Secondary Suspects

These are less likely than abandoned teardown, but still relevant:

- `MetalEditorRenderer::captureChromeFrame()` returns early when `host_->isRunning()` is false. `MetalHost::start()` can return true before peer attachment/running, so initial chrome/analyzer frame delivery depends on later timer/peer lifecycle.
- `MetalEditorRenderer::captureChromeFrame()` currently does `host_->setAnalyzerFrame(nullptr)` if `fillMetalAnalyzerFrame()` fails transiently. That can erase the last valid plot geometry/config.
- The chrome capture is JUCE `Timer` driven, so it depends on Pro Tools granting the message thread. It should eventually refresh, but it is not a reliable bootstrap source during close/reopen churn.

## Recommended Next Move

Do not add more trace rendering features yet. First harden Metal lifecycle across close/reopen.

Suggested direction:

1. Remove the `impl_.release()` abandoned-context strategy as a steady-state solution.
2. Split teardown into two phases:
   - Fast close-thread phase: mark stopping, detach the Metal layer from the peer view, restore the original JUCE backing layer if possible, clear `latestChromeFrame` / `latestAnalyzerFrame`, and prevent any more callbacks from touching the editor.
   - Async render-thread phase: invalidate `CADisplayLink`, stop the render run loop, and release Metal resources once the render thread has exited.
3. Keep any state needed by the render callback alive via shared ownership, not a leaked `MetalHostImpl` with a dangling `juce::Component&`.
4. On reopen, require a fresh `MetalHostImpl` and fresh `CAMetalLayer`; no reuse of abandoned native layer state.
5. Add temporary logs:
   - `MetalEditorRenderer::start/stop`
   - `MetalHostImpl::start/tryAttachAndStartLink/stopDisplayLinkAndDrain/detachFromPeerView`
   - Whether teardown was clean or async-pending
   - Whether `captureChromeFrame()` is skipping because `host_->isRunning()` is false

## Important Constraint

The prior close hang must not regress. Do not reintroduce an unbounded join or a main-thread call path that blocks on an in-flight Metal present.

The target is:

- Pro Tools close returns immediately.
- Old Metal layer is detached/restored before the editor is destroyed.
- Render thread/resources finish asynchronously without dangling editor references.
- Reopen always starts from a clean Metal host/layer.
