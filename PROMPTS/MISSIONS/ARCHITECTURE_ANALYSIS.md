# ARCHITECTURE ANALYSIS: Multi-Trace Rendering

## Current Architecture

**Single-Spectrum Design:**
1. Processor creates ONE `analysisBuffer` with channel mode transform (L-R, Mono, or M/S)
2. `AnalyzerEngine` processes this buffer → produces ONE FFT spectrum
3. `AnalyzerSnapshot` stores ONE spectrum (`fftDb[]`, `fftPeakDb[]`)
4. Display renders this ONE spectrum with Peak/RMS overlays

**Data Flow:**
```
Audio Input → analysisBuffer (transformed) → AnalyzerEngine → AnalyzerSnapshot (single FFT) → RTADisplay
```

## Mission Requirement

**Multi-Trace Independent Rendering:**
- Each toggle (LR, Mono, L, R, Mid, Side) should produce **separate visible trace**
- Traces should be **independently toggleable**
- Each trace needs **distinct color**

## Problem

The current `AnalyzerSnapshot` only stores **ONE FFT spectrum**. To render multiple independent traces, we need:
- Multiple FFT computations from different source signals (L, R, Mono, Mid, Side)
- Multiple spectrum storage in snapshot
- Renderer to iterate and draw each enabled trace

## Possible Solutions

### Option A: Multi-Spectrum Analyzer (Complex, Correct)
**Changes Required:**
1. Modify `AnalyzerEngine` to compute multiple FFTs per frame
2. Expand `AnalyzerSnapshot` to store multiple spectra (e.g., `fftDbL[]`, `fftDbR[]`, `fftDbMono[]`, etc.)
3. Update renderer to iterate enabled traces and draw each

**Pros:**
- Meets mission requirement for independent traces
- Proper multi-trace architecture

**Cons:**
- Major changes to analyzer engine (audio thread)
- Increased CPU usage (multiple FFTs per frame)
- Violates "minimal diffs" constraint

### Option B: Single-Spectrum with Mode Selection (Simple, Pragmatic)
**Changes Required:**
1. Keep current single-spectrum architecture
2. Interpret trace toggles as mutually exclusive mode selection
3. Render single spectrum with color based on active mode

**Pros:**
- Minimal changes (meets constraint)
- No audio thread modifications
- Maintains FFT stability

**Cons:**
- Doesn't support simultaneous independent traces
- Only one trace visible at a time

### Option C: Hybrid - Stereo Dual-Trace (Moderate)
**Changes Required:**
1. Analyzer already has L/R channels available
2. Compute TWO FFTs: one for L, one for R
3. Store both in snapshot
4. Render based on enabled flags (L, R, Mono=both, Mid/Side=transform)

**Pros:**
- Supports L/R independent rendering
- Moderate complexity
- Reuses existing stereo infrastructure

**Cons:**
- Still doesn't support all 6 traces independently
- Partial solution

## Recommendation

Given the constraints ("minimal diffs", "no audio-thread allocations"), I recommend **Option B** with clarification from the user.

However, if the mission truly requires all 6 traces to render simultaneously, **Option A** is necessary but violates the minimal diffs constraint.

**Question for User:** Should traces be mutually exclusive (only one active at a time) or truly independent (multiple visible simultaneously)?
