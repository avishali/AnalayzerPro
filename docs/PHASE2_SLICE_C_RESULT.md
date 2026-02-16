# Phase-2 / Slice C — Result

## Summary
Extracted AnalyzerEngine DSP core from AnalyzerPro into melechdsp-hq shared mdsp_dsp. AnalyzerPro now uses a thin adapter that forwards to the shared engine.

## PLAN + FILES

### melechdsp-hq
**ADDED**
- `shared/mdsp_dsp/include/mdsp_dsp/analyzer/AnalyzerEngine.h` — mdsp_dsp::AnalyzerEngine + AnalyzerEngineConfig
- `shared/mdsp_dsp/src/analyzer/AnalyzerEngine.cpp` — full DSP logic (FFT, ballistics, snapshot, multi-trace)

**MODIFIED**
- `shared/mdsp_dsp/CMakeLists.txt` — add AnalyzerEngine.cpp, link juce_audio_basics

### AnalyzerPro
**MODIFIED**
- `Source/analyzer/AnalyzerEngine.h` — thin adapter (owns mdsp_dsp::AnalyzerEngine + StereoScopeAnalyzer)
- `Source/analyzer/AnalyzerEngine.cpp` — forwards all calls to core; feeds StereoScopeAnalyzer in processBlock

### Behavior Preserved
- FFT, traces, ballistics, snapshot publication identical
- StereoScopeAnalyzer remains in product layer; adapter feeds it in processBlock
- applyPendingFftSizeIfNeeded still called from message thread (assert in adapter)
- No callsite changes (PluginProcessor, MainView, AnalyzerDisplayView unchanged)

## Constraints
- mdsp_dsp uses juce_dsp, juce_audio_basics, juce_core only (no GUI modules)
- Allocations only in prepare/initializeFFT
- mdsp_dsp depends on mdsp_core OK; mdsp_ui not involved
