---
name: Phase 4B Metal
overview: Port AnalyzerPro’s remaining multi-trace analyzer lines to the existing Metal render-thread pipeline without touching the shared analyzer engine, snapshot schema, CPU fallback, or host presentation path.
todos:
  - id: add-state-arrays
    content: Add and initialize per-trace render-thread previous/target/smoothed dB arrays in MetalHost.mm.
    status: completed
  - id: populate-pipeline
    content: Populate L/R/Mid/Side/Mono arrays from AnalyzerSnapshot using existing RMS interpolation timing.
    status: completed
  - id: route-draws
    content: Route multi-trace drawing through drawTracePayloadFromDb with existing payload fallback and visibility checks.
    status: completed
  - id: validate
    content: Run lint/build/harness validation and report any unavailable local validation steps.
    status: completed
isProject: false
---

# Phase 4B Multi-Trace Metal Plan

## Scope
- Edit only [`/Users/avishaylidani/DEV/GitHubRepo/MelechDSP/AnalyzerPro/Source/ui/analyzer/metal/MetalHost.mm`](/Users/avishaylidani/DEV/GitHubRepo/MelechDSP/AnalyzerPro/Source/ui/analyzer/metal/MetalHost.mm).
- Do not change [`AnalyzerSnapshot.h`](/Users/avishaylidani/DEV/GitHubRepo/MelechDSP/AnalyzerPro/third_party/melechdsp-hq/shared/mdsp_dsp/include/mdsp_dsp/analyzer/AnalyzerSnapshot.h), shared engine code, `AnalyzerDisplayView`, chrome capture, present/teardown, or RMS/Peak behavior.

## Retrieval Notes
- Architecture docs confirm Metal is presentation-only and must render shared state, not own feature logic.
- JUCE API check was done against local JUCE docs; no new JUCE override/signature is needed for this change.
- Reuse check: use existing `AnalyzerSnapshot` fields `fftDbLRms`, `fftDbRRms`, `fftDbMidRms`, `fftDbSideRms`, `fftDbMonoRms`. The exact DSP search query did not return a dedicated implementation, but the DSP index lists `AnalyzerSnapshot`/`AnalyzerEngine`, so no custom DSP will be written.

## Implementation
- Add five render-thread interpolation triples beside the existing RMS arrays:
  - `analyzerSmoothedDbL/R/Mid/Side/Mono`
  - `analyzerRmsPreviousDbL/R/Mid/Side/Mono`
  - `analyzerRmsTargetDbL/R/Mid/Side/Mono`
- Initialize those arrays to `-200.0f` in `initialiseRenderPipelines()`, matching the existing RMS initialization.
- In `updateAnalyzerPipelineFromSnapshot()`, inside the existing `for (i < validBins)` loop, read the five RMS-processed snapshot arrays, sanitize them, and apply only the same linear interpolation state used by RMS:
  - reset: previous = target = smoothed = source
  - new snapshot: previous = old target, target = source
  - every frame: smoothed = previous + (target - previous) * `rmsInterpolationAlpha`
- Keep peak and peak-hold math unchanged and do not add per-trace timing, ballistics, allocations, locks, or `juce::Graphics`.
- Replace the existing unconditional multi-trace draw calls in `drawAnalyzerFrame()` with pipeline-first drawing:
  - L/R/Mid/Side/Mono use their matching `analyzerSmoothedDb*` array.
  - Stereo reuses existing `analyzerSmoothedDb`.
  - If `analyzerPipelineBinsForFrame <= 1`, fall back to the existing `drawTracePayload()` path.
  - Respect each `frame->*Trace.visible` flag and keep the payloads as style/fallback data.

## Validation
- Run lints on `MetalHost.mm` after editing.
- Build `AnalyzerPro_AAX`, `AnalyzerPro_Standalone`, and `MetalReproHarness` if the configured build dirs are present.
- Run the harness in `--analyzer --multitrace --cycles 50 --frames 2000`; if ASan/NSZombie build dirs are available, run those variants too.
- PT signing/install/visual verification remains a human step per the Phase 4 prompt.