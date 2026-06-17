#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace AnalyzerPro::metal
{

class IEditorSurface
{
public:
    virtual ~IEditorSurface() = default;

    virtual bool start (juce::Component& editor) = 0;
    virtual void stop() = 0;
    virtual void resized() = 0;
    virtual bool isRunning() const noexcept = 0;
    virtual void requestChromeCapture() = 0;
};

} // namespace AnalyzerPro::metal
