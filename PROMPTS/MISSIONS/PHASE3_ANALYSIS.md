# PHASE 3 STATUS: Multi-Trace Rendering Analysis

## Mission: MULTI_TRACE_DUALFFT_V1 — PHASE 3

## Current Status

### Step 3.1: Snapshot Exposure ✅ COMPLETE
- `snapshot.powerL` and `snapshot.powerR` exposed
- Engine populates when `enableMultiTrace_` is ON

### Step 3.2-3.3: UI Rendering Analysis

**TraceConfig toggles** (already exist in RTADisplay.h:80-89):
```cpp
struct TraceConfig {
    bool showLR = true;
    bool showMono = false;
    bool showL = false;
    bool showR = false;
    bool showMid = false;
    bool showSide = false;
    bool showRMS = true;
};
```

**APVTS bindings** (already wired in AnalyzerDisplayView.cpp:589-597):
- TraceShowLR, TraceShowMono, TraceShowL, TraceShowR, TraceShowMid, TraceShowSide, TraceShowRMS

**Current rendering** (RTADisplay.cpp:1662-1680):
- ✅ RMS trace (blue, 0xff0080ff) - renders when showRMS
- ✅ Peak trace (yellow) - always renders
- ❌ L/R/Mono/Mid/Side - **NOT IMPLEMENTED**

### Required Implementation

**Option A: Full multi-trace rendering** (~200 lines)
1. Add L/R/Mono/Mid/Side data paths through UI
2. Derive Mono/Mid/Side from powerL/powerR
3. Add 5 new render blocks in paint()
4. Define distinct colors

**Option B: Minimal proof-of-concept** (~50 lines)
1. Add L trace only (separate color)
2. Validate dual-FFT works visually
3. Defer Mid/Side derivation

### Recommendation

Given time constraints, recommend **Option B** for Phase 3 completion.
Full multi-trace (Option A) can be Phase 4.

## Files That Need Modification

| File | Lines | Purpose |
|------|-------|---------|
| AnalyzerDisplayView.cpp | +30 | Pass L/R dB to RTADisplay |
| RTADisplay.h | +5 | Add setLRData method |
| RTADisplay.cpp | +50 | Render L/R traces |

**Total estimated**: ~85 lines

## STOP

Awaiting direction: Option A (full) or Option B (minimal)?
