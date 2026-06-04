#include "MetalEditorRenderer.h"
#include "../../../PluginEditor.h"

#import <QuartzCore/QuartzCore.h>

#include <cmath>
#include <cstring>

namespace AnalyzerPro::metal
{

namespace
{
constexpr double kChromeCaptureBaseIntervalMs = 100.0;
constexpr double kChromeCaptureCostMultiplier = 4.5;

int scaledDimension (int value, float scale) noexcept
{
    return juce::jmax (1, juce::roundToInt (static_cast<float> (value) * scale));
}
} // namespace

MetalEditorRenderer::MetalEditorRenderer() = default;

MetalEditorRenderer::~MetalEditorRenderer()
{
    stop();
}

bool MetalEditorRenderer::start (juce::Component& editor)
{
    stop();

    editor_ = &editor;
    host_ = std::make_unique<MetalHost>();

    auto* analyzerEditor = dynamic_cast<AnalayzerProAudioProcessorEditor*> (&editor);
    const AnalyzerEngine* analyzerEngine = analyzerEditor != nullptr ? &analyzerEditor->getMetalAnalyzerEngine() : nullptr;
    if (! host_->start (editor, MetalHostMechanism::BackingLayer, analyzerEngine))
    {
        stop();
        return false;
    }

    captureChromeFrame();
    scheduleNextChromeCapture();
    return true;
}

void MetalEditorRenderer::stop()
{
    stopTimer();
    editor_ = nullptr;

    if (host_ != nullptr)
        host_->stop();

    host_.reset();
    chromePayloadPool_.fill (nullptr);
    chromeImage_ = {};
    chromePayloadWidthPx_ = 0;
    chromePayloadHeightPx_ = 0;
    chromePayloadBytesPerRow_ = 0;
    nextChromePayloadSlot_ = 0;
    lastChromeCaptureMs_ = 0.0;
    gMetalChromeCaptureMs.store (0.0f, std::memory_order_relaxed);
    gMetalChromeCaptureIntervalMs.store (0.0f, std::memory_order_relaxed);
}

void MetalEditorRenderer::resized()
{
    if (host_ != nullptr)
        host_->resized();

    stopTimer();
    captureChromeFrame();
    scheduleNextChromeCapture();
}

bool MetalEditorRenderer::isRunning() const noexcept
{
    return host_ != nullptr && host_->isRunning();
}

void MetalEditorRenderer::timerCallback()
{
    stopTimer();
    captureChromeFrame();
    scheduleNextChromeCapture();
}

void MetalEditorRenderer::captureChromeFrame()
{
    const double captureStart = CACurrentMediaTime();
    const auto recordCaptureCost = [this, captureStart]()
    {
        lastChromeCaptureMs_ = (CACurrentMediaTime() - captureStart) * 1000.0;
        gMetalChromeCaptureMs.store (static_cast<float> (lastChromeCaptureMs_), std::memory_order_relaxed);
    };

    auto* editor = editor_.getComponent();
    if (editor == nullptr || host_ == nullptr || ! host_->isRunning())
    {
        recordCaptureCost();
        return;
    }
    if (editor->getPeer() == nullptr)
    {
        recordCaptureCost();
        return;
    }

    const int width = editor->getWidth();
    const int height = editor->getHeight();
    if (width <= 0 || height <= 0)
    {
        recordCaptureCost();
        return;
    }

    const float backingScale = juce::jmax (1.0f, host_->getBackingScaleFactor());
    constexpr float captureScale = 1.0f;
    const int widthPx = scaledDimension (width, captureScale);
    const int heightPx = scaledDimension (height, captureScale);
    if (! ensureChromePayloadPool (widthPx, heightPx, captureScale))
    {
        recordCaptureCost();
        return;
    }

    auto payload = acquireChromePayload();
    if (payload == nullptr)
    {
        recordCaptureCost();
        return;
    }

    {
        auto* analyzerEditor = dynamic_cast<AnalayzerProAudioProcessorEditor*> (editor);
        if (analyzerEditor != nullptr)
            analyzerEditor->setMetalTraceSuppressedForChromeCapture (true);

        juce::Graphics g (chromeImage_);
        g.fillAll (juce::Colours::transparentBlack);
        g.addTransform (juce::AffineTransform::scale (captureScale));
        editor->paintEntireComponent (g, true);

        if (analyzerEditor != nullptr)
            analyzerEditor->setMetalTraceSuppressedForChromeCapture (false);
    }

    juce::Image::BitmapData bitmap (chromeImage_, juce::Image::BitmapData::readOnly);
    if (bitmap.pixelStride <= 0 || bitmap.lineStride <= 0)
    {
        recordCaptureCost();
        return;
    }

    payload->widthPx = widthPx;
    payload->heightPx = heightPx;
    payload->bytesPerRow = chromePayloadBytesPerRow_;
    payload->scale = captureScale;
    payload->sequence = nextSequence_++;

    for (int y = 0; y < heightPx; ++y)
    {
        const uint8_t* source = bitmap.getLinePointer (y);
        uint8_t* dest = payload->bgraPixels.data() + static_cast<size_t> (payload->bytesPerRow) * static_cast<size_t> (y);

        if (bitmap.pixelStride == 4)
        {
            std::memcpy (dest, source, static_cast<size_t> (payload->bytesPerRow));
        }
        else
        {
            for (int x = 0; x < widthPx; ++x)
            {
                const auto colour = bitmap.getPixelColour (x, y);
                dest[(x * 4) + 0] = colour.getBlue();
                dest[(x * 4) + 1] = colour.getGreen();
                dest[(x * 4) + 2] = colour.getRed();
                dest[(x * 4) + 3] = colour.getAlpha();
            }
        }
    }

    host_->setChromeFrame (std::move (payload));

    if (auto* analyzerEditor = dynamic_cast<AnalayzerProAudioProcessorEditor*> (editor))
    {
        auto analyzerFrame = std::make_shared<MetalAnalyzerFrame>();
        if (analyzerEditor->fillMetalAnalyzerFrame (*analyzerFrame, backingScale))
            host_->setAnalyzerFrame (std::move (analyzerFrame));
        else
            host_->setAnalyzerFrame (nullptr);
    }

    recordCaptureCost();
}

void MetalEditorRenderer::scheduleNextChromeCapture()
{
    auto* editor = editor_.getComponent();
    if (editor == nullptr || host_ == nullptr)
        return;

    const double adaptiveIntervalMs = juce::jmax (kChromeCaptureBaseIntervalMs,
                                                  lastChromeCaptureMs_ * kChromeCaptureCostMultiplier);
    const int intervalMs = juce::jmax (1, static_cast<int> (std::ceil (adaptiveIntervalMs)));
    gMetalChromeCaptureIntervalMs.store (static_cast<float> (intervalMs), std::memory_order_relaxed);
    startTimer (intervalMs);
}

bool MetalEditorRenderer::ensureChromePayloadPool (int widthPx, int heightPx, float scale)
{
    if (widthPx <= 0 || heightPx <= 0)
        return false;

    const int bytesPerRow = widthPx * 4;
    const bool needsResize = chromeImage_.isNull()
        || chromePayloadWidthPx_ != widthPx
        || chromePayloadHeightPx_ != heightPx
        || chromePayloadBytesPerRow_ != bytesPerRow;

    if (! needsResize)
        return true;

    chromeImage_ = juce::Image (juce::Image::ARGB, widthPx, heightPx, true);

    std::array<std::shared_ptr<FrameTexturePayload>, kChromePayloadPoolSize> newPool;
    const auto payloadBytes = static_cast<size_t> (bytesPerRow) * static_cast<size_t> (heightPx);
    for (auto& slot : newPool)
    {
        slot = std::make_shared<FrameTexturePayload>();
        slot->widthPx = widthPx;
        slot->heightPx = heightPx;
        slot->bytesPerRow = bytesPerRow;
        slot->scale = scale;
        slot->bgraPixels.resize (payloadBytes);
    }

    chromePayloadPool_ = std::move (newPool);
    chromePayloadWidthPx_ = widthPx;
    chromePayloadHeightPx_ = heightPx;
    chromePayloadBytesPerRow_ = bytesPerRow;
    nextChromePayloadSlot_ = 0;
    return true;
}

std::shared_ptr<FrameTexturePayload> MetalEditorRenderer::acquireChromePayload() noexcept
{
    for (size_t attempt = 0; attempt < chromePayloadPool_.size(); ++attempt)
    {
        const size_t index = (nextChromePayloadSlot_ + attempt) % chromePayloadPool_.size();
        auto& candidate = chromePayloadPool_[index];
        if (candidate != nullptr && candidate.use_count() == 1)
        {
            nextChromePayloadSlot_ = (index + 1) % chromePayloadPool_.size();
            return candidate;
        }
    }

    return nullptr;
}

} // namespace AnalyzerPro::metal
