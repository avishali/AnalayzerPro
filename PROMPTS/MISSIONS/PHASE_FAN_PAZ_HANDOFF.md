# Phase Fan → PAZ-style Position Scope (handoff for Cursor)

Goal: make the Phase Fan scope look and move like Waves **PAZ Position** — a
frequency-domain stereo-position display. Mono/in-phase pushes up the center
(e.g. a kick), panned content spreads to the sides, anti-phase sits at the
horizontal extremes. Add PAZ-style numeric degree labels.

Owner decision: **Path 2 (frequency-domain)**. KEY ARCHITECTURE WIN — this does
**not** touch the audio thread, engine, or seqlock snapshot.

## Why this is low-risk (verified)
- The scope is fed by `phaseFanProvider_.pushSamples(left, right, sr)` from
  `MainView::timerCallback()` — the **message/UI thread**, pulling from a lock-free
  ring the audio thread fills (`AnalyzerPro/Source/ui/MainView.cpp` ~line 599).
- So the FFT can live **inside the provider on the UI thread**. No per-channel FFT in
  the engine, no new snapshot fields, no real-time constraints.
- Data model is already polar and PAZ-shaped:
  `melechdsp-hq/shared/mdsp_ui/include/mdsp_ui/scopes/PhaseFanRenderState.h`
  → `density[kAngleBins=180][kRadiusBins=128]`, `contourRNorm[180]`,
  `peakRNorm[180]`, `correlation`.
- Current provider (`melechdsp-hq/shared/mdsp_ui/src/scopes/PhaseFanRenderStateProvider.cpp`)
  is **time-domain per-sample**: `angle = atan2(side, mid)` per sample → density.
  This is what we replace/augment with a spectral computation.

## Conventions (verified from a screen recording of PAZ Position)
- **Top dome**, centre-bottom origin, semicircle opening up — **same geometry as the
  current scope**. Do NOT switch to a wedge.
- **0° = up = mono/in-phase**; **±90° = horizontal = anti-phase**; L/R spread between.
  Angular labels are just **Left / Right** (top corners) and **Anti-Phase** (bottom sides).
- **Radius = level in dB**, with **0 dB at the outer rim** and quiet toward the centre.
  PAZ labels the radius with a dB scale along the bottom (`0 / −20 / −40 / −60 / −80`,
  mirrored) and the concentric rings are dB rings. (NOTE: the scale is **dB, not angular
  degrees** — earlier assumption corrected.)
- Per-frequency energy is **summed per angular position** (a cloud), then ballistic-smoothed.

## Motion (observed in the recording) — this is the "musical" target
Two layers, both already present in the data model (`contourRNorm` live + `peakRNorm` hold):
- **Fast live fill:** blooms **up the centre on every beat** (bass/mono-dominant energy),
  spreading into side wings for stereo/HF content. Fast attack (~10–20 ms), moderate
  release (~150–300 ms).
- **Slow peak-hold outline:** the jagged outer contour holds the max extent and decays
  slowly over **seconds**.
Tune `persistenceMs` / contour attack-release + peak release to reproduce this.

---

## Part 1 — Spectral position in the provider (canonical melechdsp-hq)

File: `melechdsp-hq/shared/mdsp_ui/src/scopes/PhaseFanRenderStateProvider.cpp`
(+ its header for new members/config).

1. **Windowed FFT buffers (UI thread).** Maintain a fixed-size analysis buffer
   (e.g. N = 2048 or 4096 — expose as config; preallocated, no per-call alloc).
   Accumulate incoming L/R blocks; when a hop is filled, apply a Hann window and run
   **two real FFTs**: one on `mid = (L+R)/2`, one on `side = (L−R)/2`
   (or on L and R directly — see step 2). Use `juce::dsp::FFT`.

2. **Per-bin position → density.** For each bin k (skip DC / very low energy):
   - Magnitudes: `Lk = |Lc|`, `Rk = |Rc|` (or `Mk = |Mc|`, `Sk = |Sc|`).
   - **Horizontal (L↔R) from level balance / pan:**
     `pan = (Lk − Rk) / (Lk + Rk + eps)`  ∈ [−1, +1].
   - **Vertical (in-phase↔anti-phase) from phase coherence:** use the bin's
     inter-channel phase, e.g. `cosPhi = Re(Lc · conj(Rc)) / (Lk·Rk + eps)`
     (cosPhi ≈ +1 in-phase, −1 anti-phase).
   - Combine into the fan angle θ ∈ [−90°, +90°]: pan drives left/right; low
     coherence pushes the contribution toward the anti-phase extremes. A workable
     first mapping: `θ = (π/2) · pan` for position, then **fold/limit coherence**:
     bins with cosPhi < 0 bias toward ±90°. (This blend is the main tuning knob —
     iterate against the PAZ reference. Document the chosen formula.)
   - Energy weight: `e = Lk² + Rk²` (optionally perceptual: weight by 1/f or A-weighting,
     and/or compress with log/dB so quiet HF doesn't dominate — improves the "musical" read).
   - Accumulate `e` into `density[angleBin(θ)][radiusBin(level)]`, reusing the existing
     `kRScale` radius normalisation and decay/contour/peak-hold code unchanged.

3. **Keep the existing ballistics** (`applyDecay`, `updateContourFromDensity`,
   peak-hold). Tune `persistenceMs` / contour attack-release for smooth musical motion.

4. **Mode switch (decision):** add a `setAnalysisMode(TimeDomain | Spectral)` so the old
   per-sample path can stay as an option, or replace it outright. Recommend keeping both
   behind a flag during tuning, default = Spectral.

5. Keep `pushSamples(left, right, sr)` signature intact — the provider just does more
   inside. No product-side feed changes needed.

## Part 2 — Degree labels + fan geometry (product)

File: `AnalyzerPro/Source/ui/meters/PhaseFanScopeComponent.cpp`
(already draws rings + 15° spokes + L/R labels; `paint()` ~line 187).

1. **dB radial scale (the PAZ "degrees" the owner asked for).** Label the concentric rings
   in dB along the bottom diameter: `0 / −20 / −40 / −60 / −80`, mirrored left/right, with
   0 dB at the outer rim. Make the rings dB rings (match the radius mapping below).
2. Keep the **Left / Right** (top) and **Anti-Phase** (bottom-side) angular labels.
3. **Geometry: keep the current top dome** — it already matches PAZ. No `fanGeometry` change.
4. Trace colour already comes from the store (`TraceId::Peak`); leave as-is.

### Radius = dB (provider + component must agree)
Map per-position level to radius in **dB** (0 dB → rim, e.g. −80 dB → near centre) instead
of the current `1 − exp(−r·kRScale)`. Do the dB conversion where the contour radius is
normalised so the rings and the data share one scale.

---

## Propagation (Part 1 only — provider is in canonical HQ)
Same cycle as UI_OVERHAUL_03 (see PROMPTS/MISSIONS/UI_OVERHAUL_03_COMMIT_HANDOFF.md):
1. Edit canonical `melechdsp-hq`; build AnalyzerPro against it with
   `-DMELECHDSP_HQ_ROOT=/Users/avishaylidani/DEV/GitHubRepo/MelechDSP/melechdsp-hq`
   and `JUCE_PATH=/Users/avishaylidani/DEV/SDK/JUCE` (export both; needed when CMake reconfigures).
2. Iterate on the angle/energy mapping visually until it matches PAZ.
3. Commit + push HQ → bump `third_party/melechdsp-hq` submodule in AnalyzerPro → verify a
   default-path build (no `MELECHDSP_HQ_ROOT`) shows it.

## Open design points to settle while implementing
- Exact **angle blend** of pan vs phase-coherence (the core "looks like PAZ" knob).
- **Energy weighting** (linear vs perceptual/dB; per-octave normalisation) for musical feel.
- FFT **size / hop / window** (resolution vs responsiveness; kick transient should bloom fast).
- Geometry: keep top-dome or switch to bottom-centre wedge.

## Done criteria
- Phase Fan is driven by spectral position; a mono kick reads as a clear push up the
  centre; panned/wide content spreads to the sides; anti-phase sits at the horizontal
  extremes; motion is smooth/musical.
- Numeric degree labels present.
- No engine/audio-thread/snapshot changes. Canonical HQ committed + submodule bumped;
  default-path build green.
