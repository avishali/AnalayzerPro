# STEP 1 REPORT: Control Bindings Verification

## Mission: TRACE_TOGGLES_FUNCTIONALITY_FIX_V1
## Step: 1 — Verify control bindings

## Status: ✅ VERIFIED

### Control Binding Map

| Toggle | Control ID | Parameter ID | Internal Flag | Renderer Reads? |
|--------|------------|--------------|---------------|-----------------|
| Show LR | `ControlId::TraceShowLR` | `"TraceShowLR"` | `traceConfig_.showLR` | ✅ YES |
| Show Mono | `ControlId::TraceShowMono` | `"TraceShowMono"` | `traceConfig_.showMono` | ✅ YES |
| Show L | `ControlId::TraceShowL` | `"TraceShowL"` | `traceConfig_.showL` | ✅ YES |
| Show R | `ControlId::TraceShowR` | `"TraceShowR"` | `traceConfig_.showR` | ✅ YES |
| Show Mid | `ControlId::TraceShowMid` | `"TraceShowMid"` | `traceConfig_.showMid` | ✅ YES |
| Show Side | `ControlId::TraceShowSide` | `"TraceShowSide"` | `traceConfig_.showSide` | ✅ YES |
| Show RMS | `ControlId::TraceShowRMS` | `"TraceShowRMS"` | `traceConfig_.showRMS` | ✅ YES |

### Binding Flow

**1. UI Binding** ([ControlRail.cpp:129-135](file:///Users/avishaylidani/DEV/GitHubRepo/AnalyzerPro/Source/ui/layout/ControlRail.cpp#L129-L135))
```cpp
controlBinder->bindToggle (AnalyzerPro::ControlId::TraceShowLR, showLrButton);
controlBinder->bindToggle (AnalyzerPro::ControlId::TraceShowMono, showMonoButton);
controlBinder->bindToggle (AnalyzerPro::ControlId::TraceShowL, showLButton);
controlBinder->bindToggle (AnalyzerPro::ControlId::TraceShowR, showRButton);
controlBinder->bindToggle (AnalyzerPro::ControlId::TraceShowMid, showMidButton);
controlBinder->bindToggle (AnalyzerPro::ControlId::TraceShowSide, showSideButton);
controlBinder->bindToggle (AnalyzerPro::ControlId::TraceShowRMS, showRmsButton);
```

**2. Parameter Reading** ([AnalyzerDisplayView.cpp:587-597](file:///Users/avishaylidani/DEV/GitHubRepo/AnalyzerPro/Source/ui/analyzer/AnalyzerDisplayView.cpp#L587-L597))
```cpp
auto& apvts = audioProcessor.getAPVTS();
RTADisplay::TraceConfig traceConfig;
traceConfig.showLR   = apvts.getRawParameterValue("TraceShowLR")->load() > 0.5f;
traceConfig.showMono = apvts.getRawParameterValue("TraceShowMono")->load() > 0.5f;
traceConfig.showL    = apvts.getRawParameterValue("TraceShowL")->load() > 0.5f;
traceConfig.showR    = apvts.getRawParameterValue("TraceShowR")->load() > 0.5f;
traceConfig.showMid  = apvts.getRawParameterValue("TraceShowMid")->load() > 0.5f;
traceConfig.showSide = apvts.getRawParameterValue("TraceShowSide")->load() > 0.5f;
traceConfig.showRMS  = apvts.getRawParameterValue("TraceShowRMS")->load() > 0.5f;
rtaDisplay.setTraceConfig(traceConfig);
```

**3. Renderer Access** ([RTADisplay.h:79-92](file:///Users/avishaylidani/DEV/GitHubRepo/AnalyzerPro/Source/ui/analyzer/rta1_import/RTADisplay.h#L79-L92))
```cpp
struct TraceConfig
{
    bool showLR = true;
    bool showMono = false;
    bool showL = false;
    bool showR = false;
    bool showMid = false;
    bool showSide = false;
    bool showRMS = true;
};
```

### Findings

✅ **All bindings are correct**
- Each toggle button is bound to a unique APVTS parameter
- Parameters are read every frame in `timerCallback()`
- `TraceConfig` is passed to `RTADisplay` via `setTraceConfig()`
- Renderer has access to all 7 flags via `traceConfig_` member

❌ **Problem Identified**
The renderer (`paintFFTMode`) **only uses `traceConfig_.showRMS`** to conditionally render the RMS trace. It does NOT use the other flags (`showLR`, `showMono`, `showL`, `showR`, `showMid`, `showSide`) to render additional traces.

**Current Rendering Logic** ([RTADisplay.cpp:1661-1715](file:///Users/avishaylidani/DEV/GitHubRepo/AnalyzerPro/Source/ui/analyzer/rta1_import/RTADisplay.cpp#L1661-L1715)):
- Line 1662: `if (traceConfig_.showRMS)` → renders blue RMS trace
- Line 1678: `if (hasPeaks)` → renders yellow Peak trace
- **Missing**: No logic to render L, R, Mono, Mid, Side traces

### Root Cause

The L/R/Mono/Mid/Side toggles currently control the **processor's channel mode** (which signal gets analyzed), but they don't trigger **separate trace rendering** in the display. The renderer needs to be updated to:
1. Check which trace flags are enabled
2. For each enabled trace, render a separate spectrum with distinct color
3. Maintain Peak (yellow) and RMS (blue) overlays

## STOP

**Next Step**: Verify trace computation exists for each mode (Step 2)
