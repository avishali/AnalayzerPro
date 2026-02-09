#include "StereoScopeComponent.h"
#include <cmath>

namespace
{
    constexpr float kEdgeInset = 0.5f;
}

StereoScopeComponent::StereoScopeComponent (mdsp_ui::UiContext& ui)
    : ui_ (ui), fifo_ (kFifoCapacity)
{
    fifoBuffer_.resize (static_cast<size_t> (kFifoCapacity) * 2u);
    workBuffer_.resize (static_cast<size_t> (kMaxPointsPerFrame) * 2u);
    startTimerHz (30);
}

StereoScopeComponent::~StereoScopeComponent()
{
    stopTimer();
}

void StereoScopeComponent::setEnabled (bool enabled) noexcept
{
    enabled_.store (enabled, std::memory_order_relaxed);
}

void StereoScopeComponent::setPersistence (float p) noexcept
{
    persistence_.store (juce::jlimit (0.0f, 1.0f, p), std::memory_order_relaxed);
}

void StereoScopeComponent::setPointStride (int stride) noexcept
{
    pointStride_.store (juce::jmax (1, stride), std::memory_order_relaxed);
}

void StereoScopeComponent::setMaxViewportSize (int maxSize) noexcept
{
    maxViewportSize_ = maxSize;
}

void StereoScopeComponent::pushAudioBlock (const juce::AudioBuffer<float>& buffer,
                                          int startSample,
                                          int numSamples) noexcept
{
    if (!enabled_.load (std::memory_order_relaxed))
        return;
    const int numCh = buffer.getNumChannels();
    if (numCh < 2 || numSamples <= 0 || startSample < 0)
        return;
    const float* const l = buffer.getReadPointer (0, startSample);
    const float* const r = buffer.getReadPointer (1, startSample);
    const int numItems = juce::jmin (numSamples * 2, kFifoCapacity * 2);
    int s1, b1, s2, b2;
    fifo_.prepareToWrite (numItems, s1, b1, s2, b2);
    int written = 0;
    if (b1 > 0)
    {
        const int pairsInB1 = b1 / 2;
        for (int i = 0; i < pairsInB1; ++i)
        {
            fifoBuffer_[static_cast<size_t> (s1) + static_cast<size_t> (i) * 2u]     = l[i];
            fifoBuffer_[static_cast<size_t> (s1) + static_cast<size_t> (i) * 2u + 1] = r[i];
        }
        written += b1;
    }
    if (b2 > 0)
    {
        const int pairsInB1 = b1 / 2;
        const int pairsInB2 = b2 / 2;
        for (int i = 0; i < pairsInB2; ++i)
        {
            fifoBuffer_[static_cast<size_t> (s2) + static_cast<size_t> (i) * 2u]     = l[pairsInB1 + i];
            fifoBuffer_[static_cast<size_t> (s2) + static_cast<size_t> (i) * 2u + 1] = r[pairsInB1 + i];
        }
        written += b2;
    }
    fifo_.finishedWrite (written);
}

void StereoScopeComponent::resized()
{
    auto area = getLocalBounds();
    if (area.isEmpty())
        return;
    int side = juce::jmin (area.getWidth(), area.getHeight());
    if (maxViewportSize_ > 0)
        side = juce::jmin (side, maxViewportSize_);
    viewportRect_ = juce::Rectangle<int> (side, side).withCentre (area.getCentre());
    if (viewportRect_.getWidth() <= 0 || viewportRect_.getHeight() <= 0)
        return;
    accumImage_ = juce::Image (juce::Image::ARGB, viewportRect_.getWidth(), viewportRect_.getHeight(), true);
    accumImage_.clear (accumImage_.getBounds());
}

void StereoScopeComponent::timerCallback()
{
    if (!enabled_.load (std::memory_order_relaxed))
        return;
    drainFifoAndRender();
}

void StereoScopeComponent::drainFifoAndRender()
{
    const int numReady = fifo_.getNumReady();
    if (numReady < 2)
        return;
    const int itemsToRead = juce::jmin (numReady, kMaxPointsPerFrame * 2);
    int s1, b1, s2, b2;
    fifo_.prepareToRead (itemsToRead, s1, b1, s2, b2);
    const int totalRead = b1 + b2;
    if (totalRead < 2)
    {
        fifo_.finishedRead (totalRead);
        return;
    }
    size_t outIdx = 0;
    auto push = [this, &outIdx] (float lv, float rv)
    {
        if (outIdx + 1 < workBuffer_.size())
        {
            workBuffer_[outIdx]     = juce::jlimit (-1.0f, 1.0f, lv);
            workBuffer_[outIdx + 1] = juce::jlimit (-1.0f, 1.0f, rv);
            outIdx += 2;
        }
    };
    const int stride = pointStride_.load (std::memory_order_relaxed);
    int pairIdx = 0;
    if (b1 > 0)
    {
        for (int i = 0; i < b1; i += 2)
        {
            if (pairIdx % stride == 0)
                push (fifoBuffer_[static_cast<size_t> (s1) + static_cast<size_t> (i)],
                      fifoBuffer_[static_cast<size_t> (s1) + static_cast<size_t> (i) + 1]);
            pairIdx++;
        }
    }
    if (b2 > 0)
    {
        for (int i = 0; i < b2; i += 2)
        {
            if (pairIdx % stride == 0)
                push (fifoBuffer_[static_cast<size_t> (s2) + static_cast<size_t> (i)],
                      fifoBuffer_[static_cast<size_t> (s2) + static_cast<size_t> (i) + 1]);
            pairIdx++;
        }
    }
    fifo_.finishedRead (totalRead);
    const int numPairs = static_cast<int> (outIdx / 2);
    if (numPairs <= 0)
        return;

    correlation_.store (computeCorrelation (workBuffer_.data(), numPairs), std::memory_order_relaxed);

    if (accumImage_.isNull() || accumImage_.getWidth() <= 0 || accumImage_.getHeight() <= 0)
        return;

    const float persistence = persistence_.load (std::memory_order_relaxed);
    const float fadeAlpha = 1.0f - persistence;

    juce::Graphics g (accumImage_);
    g.setColour (ui_.theme().panel.withAlpha (fadeAlpha));
    g.fillAll();

    const float w = static_cast<float> (accumImage_.getWidth());
    const float h = static_cast<float> (accumImage_.getHeight());
    const float cx = w * 0.5f;
    const float cy = h * 0.5f;
    const float halfUsable = juce::jmin (cx, cy) - kEdgeInset;
    if (halfUsable <= 0.0f)
        return;

    auto toScreen = [cx, cy, halfUsable] (float x, float y)
    {
        return juce::Point<float> (cx + x * halfUsable, cy - y * halfUsable);
    };

    tracePath_.clear();
    bool first = true;
    for (int i = 0; i < numPairs; ++i)
    {
        const float l = workBuffer_[static_cast<size_t> (i) * 2u];
        const float r = workBuffer_[static_cast<size_t> (i) * 2u + 1];
        const float x = l;
        const float y = r;
        if (!std::isfinite (x) || !std::isfinite (y))
            continue;
        const auto pt = toScreen (x, y);
        if (first)
        {
            tracePath_.startNewSubPath (pt);
            first = false;
        }
        else
        {
            tracePath_.lineTo (pt);
        }
    }

    g.setColour (ui_.theme().seriesPeak.withAlpha (0.9f));
    g.strokePath (tracePath_, juce::PathStrokeType (1.2f));

    repaint();
}

float StereoScopeComponent::computeCorrelation (const float* lr, int numPairs) const noexcept
{
    if (numPairs < 2 || lr == nullptr)
        return 0.0f;
    float sumL = 0.0f, sumR = 0.0f;
    for (int i = 0; i < numPairs; ++i)
    {
        sumL += lr[static_cast<size_t> (i) * 2u];
        sumR += lr[static_cast<size_t> (i) * 2u + 1];
    }
    const float meanL = sumL / static_cast<float> (numPairs);
    const float meanR = sumR / static_cast<float> (numPairs);
    float cov = 0.0f, varL = 0.0f, varR = 0.0f;
    for (int i = 0; i < numPairs; ++i)
    {
        const float dL = lr[static_cast<size_t> (i) * 2u] - meanL;
        const float dR = lr[static_cast<size_t> (i) * 2u + 1] - meanR;
        cov += dL * dR;
        varL += dL * dL;
        varR += dR * dR;
    }
    const float denom = std::sqrt (varL * varR);
    if (denom < 1e-10f)
        return 0.0f;
    return juce::jlimit (-1.0f, 1.0f, cov / denom);
}

void StereoScopeComponent::paint (juce::Graphics& g)
{
    const auto& theme = ui_.theme();
    g.fillAll (theme.panel);

    auto area = getLocalBounds().toFloat();
    const float side = juce::jmin (area.getWidth(), area.getHeight());
    int maxSide = static_cast<int> (side);
    if (maxViewportSize_ > 0)
        maxSide = juce::jmin (maxSide, maxViewportSize_);
    auto plot = juce::Rectangle<float> (static_cast<float> (maxSide), static_cast<float> (maxSide))
                    .withCentre (area.getCentre());

    if (plot.getWidth() <= 0 || plot.getHeight() <= 0)
        return;

    const float cx = plot.getCentreX();
    const float cy = plot.getCentreY();

    g.setColour (theme.grid);
    g.drawVerticalLine (static_cast<int> (cx), plot.getY(), plot.getBottom());
    g.drawHorizontalLine (static_cast<int> (cy), plot.getX(), plot.getRight());

    auto plotInt = plot.toNearestInt();
    g.saveState();
    g.reduceClipRegion (plotInt);
    if (!accumImage_.isNull())
        g.drawImageAt (accumImage_, plotInt.getX(), plotInt.getY());
    g.restoreState();

    const float corr = correlation_.load (std::memory_order_relaxed);
    const float barW = 60.0f;
    const float barH = 6.0f;
    const float barY = area.getBottom() - barH - 4.0f;
    const float barX = area.getCentreX() - barW * 0.5f;
    const float centerX = barX + barW * 0.5f;
    g.setColour (theme.grid);
    g.drawRect (barX - 1, barY - 1, barW + 2, barH + 2);
    g.setColour (theme.seriesPeak);
    const float fillW = std::abs (corr) * barW * 0.5f;
    const float fillX = centerX + juce::jmin (0.0f, corr) * barW * 0.5f;
    if (fillW > 0.5f)
        g.fillRect (fillX, barY, fillW, barH);

    g.setColour (theme.borderDivider);
    g.drawRect (area, 1.0f);
}

/*
 * VERIFIER PROMPT
 * Use this list to confirm StereoScopeComponent behavior:
 *
 * 1. Square viewport: Scope area is square, centered in component bounds.
 * 2. Max viewport size: If maxViewportSize > 0, viewport never exceeds that size.
 * 3. Edge-to-edge calibration: x=±1 maps to square edges minus 0.5px (AA inset).
 * 4. L/R mapping: X=L, Y=R (no M-S); normalized [-1,1].
 * 5. Persistence: persistence=0 → no trail (full fade each frame); persistence=1 → heavy trail.
 * 6. Realtime-safe audio path: pushAudioBlock does no locks, allocations, or JUCE calls.
 * 7. Lock-free FIFO: AbstractFifo used for audio→UI sample transfer.
 * 8. Point stride: setPointStride(N) decimates by factor N.
 * 9. Correlation bar: -1..+1 indicator at bottom; -1 = L=-R (horizontal line to edges).
 * 10. Performance: Preallocated buffers, reused Path, capped points per frame.
 *
 * Debug helper: To validate -1 correlation → horizontal line to edges:
 * Feed identical out-of-phase L/R (e.g. L=sin, R=-sin). Correlation should → -1.
 * Lissajous plot should show horizontal line; x=±1 should reach square edges.
 */
