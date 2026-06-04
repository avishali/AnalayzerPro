#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <memory>

#include "MetalHostShared.h"

class AnalyzerEngine;

namespace AnalyzerPro::metal
{

struct MetalHostImpl;

class MetalHost final
{
public:
    MetalHost();
    ~MetalHost();

    bool start (juce::Component& editor, MetalHostMechanism mechanism, const AnalyzerEngine* analyzerEngine);
    void stop();
    void resized();
    void setChromeFrame (std::shared_ptr<const FrameTexturePayload> frame);
    void setAnalyzerFrame (std::shared_ptr<const MetalAnalyzerFrame> frame);
    float getBackingScaleFactor() const noexcept;

    bool isRunning() const noexcept;
    MetalHostMechanism getMechanism() const noexcept;

private:
    std::unique_ptr<MetalHostImpl> impl_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MetalHost)
};

} // namespace AnalyzerPro::metal
