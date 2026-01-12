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
    m[ap::control::ControlId::AnalyzerPeakHold] = "PeakHold";
    m[ap::control::ControlId::AnalyzerHold] = "Hold";
    m[ap::control::ControlId::AnalyzerPeakDecay] = "PeakDecay";
    m[ap::control::ControlId::AnalyzerDisplayGain] = "DisplayGain";
    m[ap::control::ControlId::AnalyzerTilt] = "Tilt";
    m[ap::control::ControlId::MasterBypass] = "Bypass";
    
    return [m](ap::control::ControlId id) -> juce::String
    {
        const auto it = m.find(id);
        if (it != m.end())
            return it->second;
        return {};
    };
}
