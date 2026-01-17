#include "ControlSpecs.h"
#include <array>
#include <cassert>

namespace AnalyzerPro
{

namespace
{

// Enum option tables (empty for skeleton)
[[maybe_unused]] constexpr std::array<const char*, static_cast<std::size_t>(Stage::Count)> kStageNames = {
    // Add stage names as needed
};

[[maybe_unused]] constexpr std::array<const char*, static_cast<std::size_t>(Type::Count)> kTypeNames = {
    // Add type names as needed
};

// Spec table - must match ControlId enum order exactly
constexpr std::array<Spec, static_cast<std::size_t>(ControlId::Count)> kSpecs = {
    Spec{ ControlId::AnalyzerEnable, Stage{}, Type{}, "Enable", "analyzer.enable" },
    Spec{ ControlId::MeterInGain, Stage{}, Type{}, "Gain", "meter.gain" },
    Spec{ ControlId::AnalyzerMode, Stage{}, Type{}, "Mode", "analyzer.mode" },
    Spec{ ControlId::AnalyzerFftSize, Stage{}, Type{}, "FFT Size", "analyzer.fftSize" },
    Spec{ ControlId::AnalyzerAveraging, Stage{}, Type{}, "Averaging", "analyzer.averaging" },
    Spec{ ControlId::AnalyzerHoldPeaks, Stage{}, Type{}, "Hold Peaks", "analyzer.holdPeaks" },
    Spec{ ControlId::AnalyzerPeakDecay, Stage{}, Type{}, "Peak Decay", "analyzer.peakDecay" },
    Spec{ ControlId::AnalyzerTilt, Stage{}, Type{}, "Tilt", "analyzer.tilt" },
    Spec{ ControlId::MasterBypass, Stage{}, Type{}, "Bypass", "master.bypass" },
};

// Static assert to verify order matches ControlId enum
static_assert(static_cast<std::size_t>(ControlId::Count) == kSpecs.size(),
              "Spec table size must match ControlId::Count");

} // namespace

const Spec& getSpec(ControlId id) noexcept
{
    const auto index = static_cast<std::size_t>(id);
    assert(index < static_cast<std::size_t>(ControlId::Count));
    return kSpecs[index];
}

std::string_view toStableKey(ControlId id) noexcept
{
    return getSpec(id).stableKey;
}

} // namespace AnalyzerPro
