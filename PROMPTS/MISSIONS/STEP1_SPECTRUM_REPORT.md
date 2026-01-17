# STEP 1 REPORT: Current Spectrum Storage

## Mission: MULTI_TRACE_DUALFFT_V1
## Step: 1 — Locate current spectrum storage

## Status: ✅ LOCATED

### Current Architecture

**File**: `Source/analyzer/AnalyzerEngine.cpp`
**Function**: `AnalyzerEngine::computeFFT()` (lines 161-400+)

### Spectrum Storage Format

**Complex Bins**: ❌ **NOT KEPT**
- Line 174: `fft->performRealOnlyForwardTransform(fftOutput.data(), false)`
- Complex FFT output is stored temporarily in `fftOutput` buffer
- Format: `[DC, Nyquist, real1, imag1, real2, imag2, ...]`

**Power/Magnitude**: ✅ **KEPT**
- Lines 183-195: Complex bins immediately converted to **power** (not magnitude)
- `magnitudes_[i] = real * real + imag * imag` (Power domain)
- Stored in member buffer: `std::vector<float> magnitudes_` (line 110 in .h)

**dB Values**: ✅ **KEPT**
- Power values converted to dB and stored in `AnalyzerSnapshot::fftDb[]`
- Published to UI thread via lock-free snapshot

### Current Data Flow

```
Audio Input
  ↓
FFT (real-only forward transform)
  ↓
fftOutput[] (complex bins, temporary)
  ↓
magnitudes_[] (power: re² + im², permanent)
  ↓
Frequency smoothing (applied to power)
  ↓
Ballistics (RMS/Peak smoothing)
  ↓
Convert to dB
  ↓
AnalyzerSnapshot::fftDb[] (published to UI)
```

### Key Finding

**CRITICAL**: Complex bins are **NOT preserved** after magnitude computation.

**Implication for Mission**:
- Cannot derive Mid/Side from complex bins (they're discarded)
- Must modify `computeFFT()` to:
  1. Keep complex bins for L and R channels separately
  2. Derive Mono/Mid/Side from complex bins **before** power conversion
  3. Store 5 power spectra: L, R, Mono, Mid, Side

### Storage Locations

**Current (Single Spectrum)**:
- `magnitudes_[]` - Power spectrum (member variable)
- `AnalyzerSnapshot::fftDb[]` - dB spectrum (published)
- `AnalyzerSnapshot::fftPeakDb[]` - Peak dB spectrum (published)

**Required (Multi-Spectrum)**:
- Need to store L, R, Mono, Mid, Side separately
- Options:
  - A) Expand `AnalyzerSnapshot` with 5 separate arrays
  - B) Store complex L/R, derive others on-demand
  - C) Store power L/R, derive Mono/Mid on-demand (Side requires complex)

### Recommendation

**Store complex bins for L and R** to enable proper Mid/Side derivation:
- Add `std::vector<std::complex<float>> complexL_` and `complexR_` to `AnalyzerEngine`
- Compute FFT separately for L and R channels
- Derive Mono/Mid/Side from complex bins
- Convert all 5 to power, then dB
- Expand `AnalyzerSnapshot` to store all 5 spectra

## STOP

**Next Step**: Upgrade capture to store two base spectra (L and R) with complex bins
