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
    AnalyzerPeakHold,   // Enable/disable peak hold
    AnalyzerHold,
    AnalyzerPeakDecay,
    AnalyzerDisplayGain,
    AnalyzerTilt,
    MasterBypass,
    // Add control IDs as needed
    Count
};

} // namespace AnalyzerPro
