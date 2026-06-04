#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <array>
#include <memory>

#include "IEditorSurface.h"
#include "MetalHost.h"

namespace AnalyzerPro::metal
{

class MetalEditorRenderer final : public IEditorSurface,
                                  private juce::Timer
{
public:
    MetalEditorRenderer();
    ~MetalEditorRenderer() override;

    bool start (juce::Component& editor) override;
    void stop() override;
    void resized() override;
    bool isRunning() const noexcept override;

private:
    void timerCallback() override;
    void captureChromeFrame();
    void scheduleNextChromeCapture();
    bool ensureChromePayloadPool (int widthPx, int heightPx, float scale);
    std::shared_ptr<FrameTexturePayload> acquireChromePayload() noexcept;

    static constexpr size_t kChromePayloadPoolSize = 3;

    std::unique_ptr<MetalHost> host_;
    juce::Component::SafePointer<juce::Component> editor_;
    std::array<std::shared_ptr<FrameTexturePayload>, kChromePayloadPoolSize> chromePayloadPool_;
    juce::Image chromeImage_;
    uint64_t nextSequence_ = 1;
    size_t nextChromePayloadSlot_ = 0;
    int chromePayloadWidthPx_ = 0;
    int chromePayloadHeightPx_ = 0;
    int chromePayloadBytesPerRow_ = 0;
    double lastChromeCaptureMs_ = 0.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MetalEditorRenderer)
};

} // namespace AnalyzerPro::metal
