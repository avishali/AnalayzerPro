#pragma once

namespace AnalyzerPro
{

enum class Stage
{
    // Add stages as needed
    Count
};

enum class Type
{
    // Add types as needed
    Count
};

enum class ControlId
{
    AnalyzerEnable,
    MeterInGain,
    AnalyzerMode,        // FFT / BANDS / LOG
    AnalyzerFftSize,
    AnalyzerAveraging,
    AnalyzerHoldPeaks,   // Consolidated Hold
    AnalyzerPeakDecay,
    AnalyzerTilt,
    MasterBypass,
    TraceShowLR,
    TraceShowMono,
    TraceShowL,
    TraceShowR,
    TraceShowMid,
    TraceShowSide,
    TraceShowRMS,
    AnalyzerWeighting,
    
    // Scope
    ScopeChannelMode, // 0=Stereo, 1=MidSide
    
    // Meters
    MeterChannelMode, // 0=Stereo, 1=MidSide
    
    // Add control IDs as needed
    Count
};

} // namespace AnalyzerPro
