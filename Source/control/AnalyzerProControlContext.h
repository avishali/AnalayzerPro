#pragma once

#include "ControlBinder.h"
#include "AnalyzerProParamIdMap.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include <memory>

namespace AnalyzerPro
{
namespace control
{

/**
    AnalyzerProControlContext owns the control binding infrastructure.
    Provides access to ControlBinder and UiState for UI components.
*/
class AnalyzerProControlContext
{
public:
    explicit AnalyzerProControlContext(juce::AudioProcessorValueTreeState* apvts);
    ~AnalyzerProControlContext() = default;

    ControlBinder& getBinder() noexcept { return binder; }
    const ControlBinder& getBinder() const noexcept { return binder; }

    UiState& getUiState() noexcept { return binder.getUiState(); }
    const UiState& getUiState() const noexcept { return binder.getUiState(); }

private:
    ControlBinder binder;
};

} // namespace control
} // namespace AnalyzerPro
