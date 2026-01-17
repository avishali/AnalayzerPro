# STEP 2 IMPLEMENTATION PLAN: Dual-FFT Architecture

## Mission: MULTI_TRACE_DUALFFT_V1
## Step: 2 — Upgrade capture to store two base spectra (L and R)

## Scope Analysis

This step requires **significant architectural changes** to the analyzer engine:

### Files to Modify

1. **AnalyzerSnapshot.h** - Expand to store 5 spectra (L, R, Mono, Mid, Side)
2. **AnalyzerEngine.h** - Add complex bin storage and dual-FFT infrastructure
3. **AnalyzerEngine.cpp** - Rewrite `computeFFT()` for dual-channel processing
4. **PluginProcessor.cpp** - Update to feed stereo to analyzer (currently transforms to single channel)

### Required Changes

#### 1. AnalyzerSnapshot (Data Transport)
```cpp
// Current: Single spectrum
std::array<float, kMaxFFTBins> fftDb{};
std::array<float, kMaxFFTBins> fftPeakDb{};

// Required: 5 spectra
std::array<float, kMaxFFTBins> fftDbL{};      // Left
std::array<float, kMaxFFTBins> fftDbR{};      // Right
std::array<float, kMaxFFTBins> fftDbMono{};   // Mono
std::array<float, kMaxFFTBins> fftDbMid{};    // Mid
std::array<float, kMaxFFTBins> fftDbSide{};   // Side
// + Peak versions for each
```

#### 2. AnalyzerEngine (Computation)
```cpp
// Add complex bin storage (preallocated)
std::vector<std::complex<float>> complexL_;
std::vector<std::complex<float>> complexR_;

// Add power spectrum storage for each trace
std::vector<float> powerL_;
std::vector<float> powerR_;
std::vector<float> powerMono_;
std::vector<float> powerMid_;
std::vector<float> powerSide_;

// Modify computeFFT() to:
// 1. Process L and R channels separately
// 2. Store complex bins
// 3. Derive Mono/Mid/Side from complex
// 4. Apply ballistics to each spectrum
```

#### 3. PluginProcessor (Input Routing)
```cpp
// Current: Transforms to single channel before analyzer
// analysisBuffer → channel mode transform → analyzer

// Required: Feed stereo to analyzer
// analysisBuffer (L/R untransformed) → analyzer
// Analyzer handles all derivations internally
```

### Complexity Assessment

**Estimated Changes:**
- ~200 lines in AnalyzerEngine.cpp (rewrite computeFFT)
- ~50 lines in AnalyzerEngine.h (new members)
- ~30 lines in AnalyzerSnapshot.h (expand storage)
- ~20 lines in PluginProcessor.cpp (routing change)

**Risks:**
- Breaking FFT stability (mission constraint)
- Audio thread allocations (mission constraint)
- Increased CPU usage (2 FFTs instead of 1)

### Constraint Violations?

**"Minimal diffs"**: ❌ This is NOT minimal (~300 lines)
**"No audio-thread allocations"**: ✅ Can maintain (preallocate in prepare)
**"Maintain FFT stability"**: ⚠️ Risk (major rewrite of computeFFT)

## Recommendation

This step violates the "minimal diffs" constraint. The dual-FFT architecture requires substantial changes to core analyzer code.

**Options:**
1. **Proceed** - Accept that this is not "minimal" but necessary for the feature
2. **Simplify** - Use power-only storage (can't do Side correctly)
3. **Abort** - Mission requirements incompatible with constraints

**Question for User**: Should I proceed with the full dual-FFT implementation (~300 line change)?
