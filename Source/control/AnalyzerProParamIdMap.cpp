#include "AnalyzerProParamIdMap.h"
#include <unordered_map>

ap::control::ParamIdMap ap::control::makeDefaultParamIdMap()
{
    std::unordered_map<ap::control::ControlId, juce::String> m;
    m[ap::control::ControlId::AnalyzerEnable] = "Enable";
    m[ap::control::ControlId::MeterInGain] = "Gain";
    m[ap::control::ControlId::AnalyzerMode] = "Mode";
    m[ap::control::ControlId::AnalyzerFftSize] = "FftSize";
    m[ap::control::ControlId::AnalyzerAveraging] = "Averaging";
    m[ap::control::ControlId::AnalyzerHoldPeaks] = "HoldPeaks";
    m[ap::control::ControlId::AnalyzerPeakDecay] = "PeakDecay";
    m[ap::control::ControlId::AnalyzerTilt] = "Tilt";
    m[ap::control::ControlId::MasterBypass] = "Bypass";
    
    // Trace Controls
    // Trace Controls
    m[ap::control::ControlId::TraceShowLR]   = "TraceShowLR"; // Kept as legacy/bridge
    m[ap::control::ControlId::TraceShowMono] = "analyzerShowMono";
    m[ap::control::ControlId::TraceShowL]    = "analyzerShowL";
    m[ap::control::ControlId::TraceShowR]    = "analyzerShowR";
    m[ap::control::ControlId::TraceShowMid]  = "analyzerShowMid";
    m[ap::control::ControlId::TraceShowSide] = "analyzerShowSide";
    m[ap::control::ControlId::TraceShowRMS]  = "analyzerShowRMS";
    m[ap::control::ControlId::AnalyzerWeighting] = "analyzerWeighting";
    m[ap::control::ControlId::ScopeChannelMode]  = "scopeChannelMode";
    m[ap::control::ControlId::MeterChannelMode]  = "meterChannelMode";
    
    return [m](ap::control::ControlId id) -> juce::String
    {
        const auto it = m.find(id);
        if (it != m.end())
            return it->second;
        return {};
    };
}
