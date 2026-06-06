# GPU Plugin Rendering Playbook (macOS / Metal in a JUCE audio plugin)

Reusable knowledge distilled from the AnalyzerPro "Apple Metal" effort. The goal: render a
plugin's dynamic visuals (analyzers, scopes, meters, visualizers) on the GPU at **display rate**,
escaping the host's UI-thread throttle — without the crash/teardown/perf traps we hit. Read this
before starting GPU work on the next plugin; it will save many host build/sign/test cycles.

This is product-agnostic; AnalyzerPro is the worked example.

---

## 1. Why bother (the payoff and the core insight)

- **Hosts throttle the UI thread.** In Pro Tools during playback, the host grants the JUCE
  MessageManager lock only **~13×/sec**. Anything painted through the JUCE component tree (CPU *or*
  `OpenGLContext` component painting) is capped at ~13 fps. Lowering paint rates does nothing — the
  wall is host scheduling.
- **The display/GPU render thread runs free at ~60–120 fps** inside the host, independent of the
  starved message thread.
- **The smoothness comes from running BALLISTICS at display rate**, not from raw data rate. The
  engine publishes new data at ~30/sec; if the render thread advances attack/release/peak-hold
  *per frame* toward the latest data, a 120 fps thread produces smoothly interpolated motion. This
  is THE reason to move rendering to the GPU thread. (If you only redraw the same data faster, it's
  still choppy — see §7.)

## 2. Architecture: full-editor Metal (Approach B). NOT a child overlay.

- **A child/overlay `CAMetalLayer` blanks the entire host editor** (proven 3 ways in Pro Tools,
  including JUCE's own Metal backing). **The GPU must own the WHOLE editor surface.**
- **Mechanism = BackingLayer:** swap the editor's top-level peer `NSView.layer` for your
  `CAMetalLayer` (`getPeer()->getNativeHandle()` → `NSView`, `wantsLayer=YES`). JUCE keeps its
  component tree and native event routing; you only take over presentation. Native input keeps
  working (you replaced the layer, not the event path).
- **Consequence:** once your layer is the backing layer, **JUCE no longer paints to screen.** You
  must capture the JUCE UI yourself (chrome texture, §6) and composite it, or the editor is blank.
- Composite every frame: (1) the **chrome texture** (the whole JUCE UI rendered offscreen at the
  message-thread rate — fine, it doesn't need 60 fps), drawn as a full-surface quad; (2) the
  **dynamic content** (analyzer/scope) drawn fresh as GPU geometry at display rate, over its rect.

## 3. The render driver — what works, what doesn't (we tried all three)

- **✅ Self-paced render thread looping on `nextDrawable`.** A dedicated thread runs
  `while(!stopping){ @autoreleasepool{ drawable=[layer nextDrawable]; if(!drawable){sleep;continue;}
  ...encode...; present; } }`. `nextDrawable` **blocks ~vsync** (it paces you to display rate) and
  with `allowsNextDrawableTimeout=YES` it returns after ~1s if offscreen, so the loop stays
  responsive. **Teardown = set a flag; the loop exits within one drawable-timeout; bounded join.**
  No run loop, no AppKit lifecycle coupling. This is the driver to use.
- **❌ `CADisplayLink` on a background run loop.** A **view-vended** link is auto-suspended by
  AppKit when it can't track the plugin NSView's on-display state in a host → it **never ticks**
  (0 fps). And its `invalidate`/`CFRunLoopStop` teardown can bounce to the main thread and **never
  stop** → render threads get abandoned → zombie accumulation → crash. Avoid.
- **❌ `CVDisplayLink`.** Rendered fine, but `CVDisplayLinkStop()` is **synchronous and deadlocks**
  at teardown (blocks the main thread on the IO mutex while the in-flight callback is blocked
  releasing the presented drawable, which needs a main-thread CA commit). Deprecated on macOS 14+.
  Avoid.

> A high `nextDrawable`-blocked % (e.g. 97%) is **normal vsync idle**, NOT starvation — at 120 fps
> each frame is ~8.3 ms of which encode is ~0.1 ms and the rest is waiting for vsync. Measure
> `present_fps`, not block %. If `present_fps` ≈ display rate, the present path is healthy.

## 4. Present path

- **Use async `presentDrawable:` with `presentsWithTransaction = NO`** for a BackingLayer.
- **❌ `presentsWithTransaction = YES` blocks on frame 1** for a backing layer: the present's
  completion is tied to the window's **main-thread** CoreAnimation transaction, which the starved
  host main thread can't service → the render thread parks → black window. Don't use it here.
- Drop any contradictory `[CATransaction begin]…commit]` wrap around an async present.

## 5. Lifecycle / teardown (this is where hosts crash)

- **Detach-first teardown.** On editor close / peer loss, FIRST restore the peer's original backing
  layer (synchronous, fast, must not block) — this orphans your `CAMetalLayer` from the view so the
  in-flight present no longer needs the main thread → it completes and the render thread exits
  promptly. THEN stop+drain the render thread with a **bounded** join. Never block the main thread
  on an in-flight present.
- **Hook visibility/peer loss**, not just the destructor (`componentPeerChanged` when
  `getPeer()==nullptr`, visibility-hidden). Stop there, while the run loop is still alive.
- **Process-wide live-render-thread counter** (`++` on thread start, `--` on exit, log it) so a
  close/reopen soak can confirm it returns to **0** — no abandoned threads.
- Idempotent stop; safe to call repeatedly and in any order.

## 6. Chrome capture (the JUCE UI → texture) — cost is the trap

- Render the editor's component tree to an offscreen `juce::Image` (`paintEntireComponent`), upload
  as a texture, draw as a full-surface quad. Suppress only the *moving* trace in the capture (the
  GPU draws that); keep grid/labels/background in the chrome.
- **It will hang the host if it's too expensive.** Proven: two animated vector scopes re-stroked
  every capture saturated PT's main thread → host hang, render thread starved → blank screen.
  Mitigations: **self-rescheduling time-boxed capture** (never a fixed-rate Timer; schedule next =
  `max(base, lastCaptureMs * ~4)` so it can't run back-to-back); capture at **logical (1×)
  resolution** and let the GPU upscale (cuts Retina cost ~4×); **pool** the payload buffers (no
  per-capture multi-MB alloc); skip when `getPeer()==nullptr`.
- **Double/triple-buffer the chrome texture** (ring of 3): uploading into the texture the render
  pass is sampling = CPU/GPU race → tearing/flash.

## 7. Data handoff & the render-thread pipeline (this is where smoothness lives)

- **Lock-free seqlock snapshot** from the engine; the render thread calls `getLatestSnapshot()`
  **every frame** (safe off the message thread; never touch the audio thread or DSP).
- **Run the per-frame pipeline on the render thread**: dB conversion / weighting / smoothing /
  **ballistics & peak-hold advanced by REAL elapsed time** (`CACurrentMediaTime()` delta, not an
  assumed tick rate). State is owned exclusively by the render thread; never shared with the
  message thread.
- **Message thread publishes immutable CONFIG ONLY** (plot rect in px, freq range, dB window,
  colors, trace enables) via `std::atomic<std::shared_ptr<const T>>`. Not the spectrum.
- **No `juce::Graphics`, no MessageManager lock, no per-frame allocations on the render thread.**
  Preallocate vertex buffers to max bins; reuse.
- **Gotcha we hit:** the pipeline was *implemented but never wired into the draw path* — the draw
  kept using the message-thread-published trace vectors (~13 fps) and the render-thread pipeline was
  dead code. Symptom: `present_fps=120` but the trace was as choppy as CPU. **Verify the draw
  actually consumes the render-thread pipeline output** (add a `pipeline_fps` counter; it must
  track `present_fps`).

## 8. Objective-C memory management (ARC is OFF in `.mm`) — the crash class

- **Convenience/factory methods return AUTORELEASED objects you do NOT own** — never release them.
  The bug that cost the most: `[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:…]` is
  autoreleased; a stray `[descriptor release]` over-released it → `EXC_BAD_ACCESS` in `objc_release`
  at the next `@autoreleasepool` pop. Rule: only `release` what you got from `alloc`/`new…`/`copy`/
  explicit `retain`.
- **Cross-thread drawable ownership**: retain the drawable when you take it, release exactly once
  (e.g. in the command-buffer completion handler); under MRC a block does NOT auto-retain a captured
  ObjC pointer on copy.
- **`CAMetalDrawable` released at `@autoreleasepool` pop** was the recurring problem child for both
  the CVDisplayLink deadlock and over-release crashes — keep its ownership explicit and bounded.

## 9. Diagnosing in a plugin host (hosts hide everything)

- **Crashes don't reach Console.** Pro Tools bundles **Google Crashpad**, which intercepts the
  exception before Apple's ReportCrash → nothing in Console/`DiagnosticReports`. Find the minidump
  in `~/Library/Application Support/Avid/Pro Tools/Crashpad/completed/*.dmp` and walk it with
  **lldb (no relaunch):** `lldb -b -o 'target create -c <dmp>' -o 'thread backtrace' -o quit`.
- **Host NSLog may not reach `log show`.** Use a **file-based stats sink** the render thread writes
  once/sec (`/tmp/…stats.txt`, atomic temp+rename, stack buffers, no per-frame FS). On-screen HUDs
  aren't always visible in a host. (Don't ship the FS-on-render-thread writer — gate it debug-only.)
- **CPU-vs-GPU / "is it even running":** `sample <host-pid>` and grep the render-thread frames.
  A thread parked in `[CAMetalLayer nextDrawable]` is healthy vsync idle, not a hang.
- **Stale-build / stale-sign roulette:** the install pipeline can re-sign an OLD compile. Embed a
  `__DATE__ __TIME__` stamp and gate: before signing, the binary stamp must postdate the source
  edit. **Caveat:** the stamp only bumps on a *clean* build (the stamp file isn't recompiled by an
  incremental build) — so for incremental builds confirm the change is in via `strings <binary> |
  grep <unique-new-string>` instead of trusting the stamp. Verify installed SHA == signed artifact.

## 10. The headless harness (build it early — it pays for itself)

- Build a tiny executable that drives the **real** render path outside the host: an on-screen
  `NSWindow` + the real `MetalHostImpl`, a synthetic chrome publisher, and a synthetic engine
  feeder, looped over many construct→run→stop cycles, runnable under **NSZombie / ASan /
  MallocScribble**. It catches ObjC over-release / lifetime bugs the host hides, with no
  sign/install cycle, and names the offending object (e.g. via NSZombie). It found the descriptor
  over-release in one run after weeks of host roulette.
- **Bisect toggles** (`--empty` = drawable-only, `--chrome`, `--analyzer`, `--multitrace`) localize
  a bug to drawable / chrome / pipeline / specific draw path fast.
- **Limits:** the harness window composites at full vsync, so it CANNOT reproduce host-specific
  compositing/starvation behavior — those must be measured in the host (with the file stats sink).
  And "vertices built" (`build_ok`) is not "pixels visible" — confirm visually or it'll mislead.

## 11. Packaging / platform

- Behind a CMake flag (`*_METAL_EDITOR`, macOS only) + the `IEditorSurface` interface, with the CPU
  editor as permanent fallback. Runtime `wrapperType` gate per format (start AAX-only; widen per
  host only after per-host validation). Never `#if JucePlugin_Build_AAX`.
- Keep a runtime **kill-switch env var** (e.g. `*_DISABLE_AAX_METAL=1`) so a tester can fall back to
  CPU without a rebuild.
- See `ARCHITECTURE_platforms.md` for the one-codebase / shared-features principle.

## 12. The "do NOT retry" list (each cost real host build/sign/test cycles)

- Child/overlay `CAMetalLayer` (blanks the host editor).
- `presentsWithTransaction = YES` on a backing layer (blocks frame 1, black).
- View-vended `CADisplayLink` on a background run loop (never ticks in-host; won't tear down).
- `CVDisplayLink` (synchronous-stop teardown deadlock).
- Tuning `CAMetalDrawable` retain/release timing in the host to chase a crash — reproduce it in the
  harness under NSZombie/ASan instead; the failing object must be NAMED before you touch timing.
- Trusting the build stamp on an incremental build; trusting "it ran" claims over the on-disk
  binary/logs.
- Assuming `build_ok`/vertices-built means the trace is visible (it can be opaque + drawn but
  overdrawn, or out of the dB/freq window).
