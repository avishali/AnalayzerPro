# Phase 4 — Port remaining surfaces to Metal + distribution decision (Cursor prompts)

Ships on top of **AnalyzerPro 1.2.0 "Apple Metal"** (tag `v1.2.0`, branch `master` @ `1437de1`,
SDK `melechdsp-hq` master @ `1b378d1`). At 1.2.0, the GPU AAX editor (macOS) renders the **chrome
texture** + the **RMS / Peak / peak-hold** analyzer traces on the render thread at display rate.
Phase 4 moves the **remaining** surfaces off the slow chrome path onto the GPU, decides the
**meters** policy, and settles **distribution signing**.

Read `docs/GPU_PLUGIN_RENDERING_PLAYBOOK.md` and `docs/ARCHITECTURE_platforms.md` first — every
prompt below assumes those rules.

---

## §0 CONTEXT (prepend to every Phase-4 prompt)

You are in the AnalyzerPro JUCE 8 plugin (CMake, macOS/AppleSilicon). The macOS GPU editor is
**on by default for AAX** behind `ANALYZERPRO_METAL_EDITOR`, kill-switch
`ANALYZERPRO_DISABLE_AAX_METAL=1`. Non-macOS / non-AAX = CPU editor (unchanged).

**Non-negotiable rules (proven this project — do not relearn the hard way):**
- **One codebase. Metal is a macOS presentation layer, not a fork.** Features live in shared core
  (`mdsp_*` in `third_party/melechdsp-hq`); the GPU path only *renders* shared engine/provider
  state. **Never put feature logic in `MetalHost.mm`** — Mac and Windows must stay identical.
- **Render thread:** no MessageManager lock, no `juce::Graphics`, no per-frame allocations. Read
  **lock-free** providers/snapshots only; preallocate buffers, reuse.
- **Smoothness = render-thread interpolation matched to the REAL data rate.** Measure the actual
  inter-update interval dynamically (don't hardcode a Hz); interpolate over it. Do **not** add a
  second ballistics/attack-release stage on top of data the engine already smoothed (that caused
  the "slow motion" bug). Mirror the CPU's per-trace processing exactly so Mac == Windows feel.
- **ARC is OFF in `.mm`.** Convenience/factory methods (`…Descriptor`, `commandBuffer`,
  `…Encoder`, `nextDrawable`) return **autoreleased** objects — never `release` them. Only release
  what you `alloc`/`new…`/`copy`/`retain`. (The 1.2.0 crash was a stray descriptor over-release.)
- **Don't touch** the present path, teardown, descriptor handling, BackingLayer hosting, or the
  RMS/Peak pipeline that 1.2.0 shipped.
- **Per-format = runtime `wrapperType` check**, never `#if JucePlugin_Build_AAX`.
- **Validate lifetime in the harness (NSZombie/ASan), not in Pro Tools.** "Vertices built" ≠
  "pixels visible" — confirm visually too.

**Build / sign / install / verify recipe (the human runs sign+install; Cursor builds + harness-validates):**
```sh
# build (release universal, Metal on)
JUCE_PATH=/Users/avishaylidani/DEV/SDK/JUCE AAX_SDK_PATH=/Users/avishaylidani/Downloads/aax-sdk-2-8-0 \
  cmake --build build-release-macos-universal --target AnalyzerPro_AAX AnalyzerPro_Standalone
# footer git-hash is CONFIGURE-TIME: after a commit, reconfigure so it refreshes:
JUCE_PATH=… AAX_SDK_PATH=… cmake build-release-macos-universal   # then rebuild
# sign (PACE cloud sign via scripts/.aax_wraptool.env) + install + verify:
A="build-release-macos-universal/AnalyzerPro_artefacts/Release/AAX/AnalyzerPro.aaxplugin/Contents/MacOS/AnalyzerPro"
bash scripts/wraptool_sign_aax.sh "$A"
rsync -a --delete "$(dirname "$A")/../.." "/Library/Application Support/Avid/Audio/Plug-Ins/AnalyzerPro.aaxplugin/"
# verify: installed SHA == built SHA; footer git-hash matches HEAD; codesign -v --strict passes
```
- **Build identity:** trust the **git short-hash in the footer** (not the `__DATE__/__TIME__`
  stamp — it only refreshes on a clean/reconfigured build). PT caches the loaded binary — **reinsert
  the plugin / restart PT** after install to load a new build.
- **Harness:** `cmake --build build-metal-repro --target MetalReproHarness` (NSZombie) and
  `build-asan` (ASan). Run all toggles `--cycles 50 --frames 2000`; require zero zombies/ASan
  reports and `live_render_threads`→0 each cycle.

End of CONTEXT.

---

## Phase 4A — Goniometer + Phase-fan scopes → GPU (DO THIS FIRST)

**Why first:** the two animated vector scopes (`StereoScopeComponent` goniometer,
`PhaseFanScopeComponent`) are re-stroked into the **chrome texture every capture** — the dominant
chrome-capture cost (they once *hung* Pro Tools via `strokePath` saturating the main thread). The
**primary goal of 4A is to remove that chrome `strokePath` cost**, not to hit 120 fps scope data.

**Corrected facts (a first attempt failed on these — do not repeat):**
- **The scope providers are NOT lock-free.** `StereoScope`/`PhaseFanRenderStateProvider` expose a
  plain `state()` getter, fed by `pushSamples()`, read on the **message thread** (MainView). Do
  **NOT** read `state()` from the render thread (race). **Publish an immutable scope snapshot from
  the message thread** (`std::atomic<std::shared_ptr<const MetalScopeFrame>>`), built from
  `state()` on the message thread — same pattern as the analyzer config. Message-rate scope data is
  acceptable (scopes aren't motion-critical); the win is killing the chrome strokePath cost.
- **Scale MUST be unified on the host backing scale.** Use the analyzer/Metal path's
  `host_->getBackingScaleFactor()` for ALL scope geometry — **never** `editor->getDesktopScaleFactor()`
  (that mismatch was the placement bug; fixing it placed the phase-fan correctly).
- **Goniometer = points, not line-strips** (line strips produced spike artifacts) — draw it as a
  point cloud (small quads / point primitives).

**Prompt — STEP 0 (diagnostic, do this alone first; do NOT draw real scopes yet):**

> Add a temporary debug mode that draws a solid translucent **rectangle** in Metal at each scope's
> computed rect — phase-fan rect and goniometer rect — using the SAME rect→drawable-pixel mapping
> the analyzer plot rect uses (host `getBackingScaleFactor()`, editor-coords → drawable px). Also
> overlay the analyzer plot rect for reference. Do NOT suppress the chrome scopes yet (so you see the
> Metal debug rect AND the CPU scope together). Build/sign/install/PT: **confirm each Metal debug
> rect exactly covers where the CPU scope is.** Iterate ONLY on the rect math until they align
> pixel-for-pixel on a Retina display. Do not touch the spectrum/analyzer draw path. ⛔ STOP — nail
> placement before any real geometry.

**Prompt — STEP 1 (one surface: phase-fan first, then goniometer):**

> With rects confirmed (Step 0), port **one scope at a time, phase-fan first**. (1) Message thread:
> build an immutable `MetalScopeFrame` (point/segment arrays in normalized scope space + rect in
> drawable px + colors/mode from theme) from the provider's `state()`, publish via
> `atomic<shared_ptr<const MetalScopeFrame>>`. (2) Render thread: draw it into its rect (scissor),
> matching the CPU look (colors from theme/`TraceColors`, blend, fade); phase-fan as line/segments,
> **goniometer as points (small quads), NOT line-strips**. Preallocated buffers; no
> alloc/`juce::Graphics`/MM-lock on the render thread; ARC-off (don't release autoreleased objects).
> Do NOT add a second ballistics stage. **Do NOT touch the spectrum/analyzer pipeline** (the prior
> attempt regressed it by interacting with the spectrum-overlay path — keep them fully isolated).
> Once a scope renders correctly in Metal, **suppress that scope in the chrome capture** (flag, like
> the analyzer trace) so it's not double-drawn and chrome cost drops. Keep CPU scope paint as the
> permanent fallback. Validate in the harness `--scopes` mode (NSZombie+ASan, clean) then PT. ⛔
> STOP after phase-fan; human signs + PT-verifies BEFORE starting the goniometer.

**Acceptance (per scope):** placed correctly on 1× and 2× displays; looks like the CPU scope; PT
main-thread cost drops (scope no longer stroked into chrome); no hang on multi-instance churn;
spectrum/analyzer unchanged.

---

## Phase 4B — Remaining analyzer traces (L / R / M / S / stereo / mono) → render thread

**Why:** RMS/Peak are smooth (1.2.0), but the multi-traces still come from message-thread
`multiTraceLFrame_/R/Mid/Side/Mono` + `stereo/mono` payloads (`AnalyzerDisplayView`), so they
animate at the ~13 fps message rate during playback. Port them to the render-thread pipeline like
RMS/Peak.

**Dependency to resolve first:** the render thread needs **lock-free** access to the L/R/M/S/
stereo/mono spectra. Today those are published as message-thread `*Frame_.display_` vectors. If the
shared engine doesn't already expose them in a lock-free snapshot, add that to the **shared engine**
(not the Metal layer) — extend the seqlock snapshot with the multi-trace spectra so the render
thread can read them, exactly as it reads the RMS `fftDb`.

**Prompt:**

> Move the remaining analyzer traces (stereo, mono, L, R, Mid, Side) onto the Metal render-thread
> pipeline, like RMS/Peak. Step 1: ensure the **shared engine** publishes these spectra in the
> lock-free snapshot the render thread already reads (extend the snapshot in
> `mdsp_dsp`/`AnalyzerSnapshot` if needed — shared core, NOT the Metal layer; this also benefits the
> CPU path / Windows). Step 2: in `MetalHost.mm`, draw each enabled trace via the existing
> `drawTracePayloadFromDb` path from the render-thread snapshot data, with **dynamic interpolation
> matched to the real data rate** (reuse the RMS interval-measurement) and the trace's
> color/fill/stroke from the published `RenderConfig`/theme — **respect each trace's `visible`
> toggle** (do not hard-code visibility). Keep `displayGainDb`/tilt compensation consistent with the
> CPU. Leave the published-db path as the no-engine fallback. No double-smoothing, no per-frame
> alloc. **Validate:** harness `--analyzer --multitrace` clean (NSZombie+ASan), all traces visible
> and smooth, `rms_above_peak`-style invariants hold; then PT. ⛔ STOP; human signs + verifies.

---

## Phase 4C — Meters: GPU or stay in chrome? (decision + document)

Meters have a lock-free provider (`mdsp_ui::MeterRenderStateProvider` / `MeterSnapshot` /
`MeterRenderState`). They're cheap level bars; the chrome rate (~10–15 fps) is often fine for
meters. Decide with measurement, don't port reflexively.

**Prompt:**

> Evaluate the meters under the Metal editor during PT playback: do they feel laggy at the
> chrome-capture rate? If acceptable, **keep them in the chrome texture** and write a one-paragraph
> rationale in `docs/`. If laggy, port them to GPU geometry (simple colored quads per segment) on
> the render thread reading `MeterRenderStateProvider` (lock-free), suppressed in the chrome
> capture, matching the CPU meter look (scale, peak-hold tick, colors). Same render-thread rules as
> 4A/4B. Document the decision either way. ⛔ STOP; if ported, human signs + verifies.

---

## Phase 4D — Distribution: PACE sign vs wrap (decision, then packaging)

The 1.2.0 dev/test builds are PACE **signed** (`wraptool sign` via `scripts/wraptool_sign_aax.sh`)
— enough for retail Pro Tools to *load* the plugin. **Wrapping** (`wraptool wrap`) is the separate
PACE **copy-protection / licensing** layer. The beta portal (`beta.melech-dsp.com`) serves signed
installers to testers.

**Decision to make (human):**
- **If AnalyzerPro is free / unprotected for beta** → **sign-only is correct** (what we ship now).
  No wrap step; the existing `wraptool_sign_aax.sh` flow is the distribution flow.
- **If AnalyzerPro uses PACE licensing / copy-protection** → the distributed build must be
  **wrapped** (not just signed). That needs a `wraptool wrap` step with the licensing config
  (activation/trial policy, Fusion wrap config GUID) — a different command than `sign`.

**Prompt (only if wrapping is required):**

> Add a release **wrap** path alongside the existing sign script: a `scripts/wraptool_wrap_aax.sh`
> using `wraptool wrap` with the PACE licensing/Fusion config (account/password from
> `scripts/.aax_wraptool.env`, wrap-config GUID, trial/activation policy). Keep `sign` for dev
> builds; use `wrap` for the distributed beta build. Verify with `wraptool verify` that the shipped
> bundle reports **wrapped** (not just "signed, but not wrapped"). Wire it into the release/installer
> flow that feeds the beta portal. Document the licensing policy.

**If sign-only:** no code change — confirm the beta installer uses the existing signed AAX, and note
in the release notes that 1.2.0 is sign-only.

---

## Cross-cutting reminders (every Phase-4 task)

- Shared features in `mdsp_*`; Metal renders, never owns feature logic. (`ARCHITECTURE_platforms.md`)
- Render thread: lock-free reads, dynamic interpolation matched to real rate, no double-smoothing,
  no alloc / `juce::Graphics` / MM-lock. ARC-off: don't release autoreleased objects.
- Suppress each ported surface in the chrome capture (avoid double-draw + cut chrome cost).
- CPU paths stay intact + flag-gated as the permanent fallback (Windows + Metal-off).
- Validate lifetime in the harness (NSZombie/ASan) before PT; confirm pixels visually, not just
  vertex counts. Don't trust the date stamp — use the git-hash footer; reinsert in PT after install.
- One surface at a time, each its own ⛔ STOP + human sign + PT verify, each leaving the plugin
  shippable (flag off ⇒ current behavior).
- After each surface lands + PT-verifies: commit (and bump patch version if shipping), keep the SDK
  submodule pin current (`master`), push — don't let the SDK drift (it's shared with other plugins).
