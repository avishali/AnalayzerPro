#include "StereoScopeComponent.h"

#include "../theme/TraceColors.h"

#include <cmath>

namespace
{
constexpr float kEdgeInset = 0.5f;

juce::Colour traceColourOrFallback (AnalyzerPro::TraceColorStore* store,
                                    AnalyzerPro::TraceId id,
                                    juce::Colour fallback)
{
    return store != nullptr ? store->get (id) : fallback;
}
}

StereoScopeComponent::StereoScopeComponent (mdsp_ui::UiContext& ui)
    : ui_ (ui)
{
}

void StereoScopeComponent::setEnabled (bool enabled) noexcept
{
    enabled_ = enabled;
    repaint();
}

void StereoScopeComponent::setMaxViewportSize (int maxSize) noexcept
{
    maxViewportSize_ = maxSize;
    resized();
}

void StereoScopeComponent::setRenderState (const mdsp_ui::scopes::StereoScopeRenderState& state)
{
    state_ = state;
    rebuildCachedPaths();
    repaint();
}

void StereoScopeComponent::setFreezeToggleCallback (ScopeActionFn fn, void* ctx) noexcept
{
    onFreezeToggle_ = fn;
    freezeCtx_ = ctx;
}

void StereoScopeComponent::setResetCallback (ScopeActionFn fn, void* ctx) noexcept
{
    onReset_ = fn;
    resetCtx_ = ctx;
}

void StereoScopeComponent::triggerFreezeToggle() noexcept
{
    if (onFreezeToggle_ != nullptr)
        onFreezeToggle_ (freezeCtx_);
}

void StereoScopeComponent::triggerReset() noexcept
{
    if (onReset_ != nullptr)
        onReset_ (resetCtx_);
}

void StereoScopeComponent::resized()
{
    auto area = getLocalBounds();
    if (area.isEmpty())
    {
        viewportRect_ = {};
        cachedLivePath_.clear();
        cachedHoldPath_.clear();
        for (auto& path : cachedHistoryPaths_)
            path.clear();
        return;
    }

    int side = juce::jmin (area.getWidth(), area.getHeight());
    if (maxViewportSize_ > 0)
        side = juce::jmin (side, maxViewportSize_);

    viewportRect_ = juce::Rectangle<int> (side, side).withCentre (area.getCentre());
    rebuildCachedPaths();
}

void StereoScopeComponent::rebuildCachedPaths()
{
    cachedLivePath_.clear();
    cachedHoldPath_.clear();
    for (auto& path : cachedHistoryPaths_)
        path.clear();

    if (viewportRect_.isEmpty())
        return;

    const float w = static_cast<float> (viewportRect_.getWidth());
    const float h = static_cast<float> (viewportRect_.getHeight());
    const float cx = static_cast<float> (viewportRect_.getX()) + w * 0.5f;
    const float cy = static_cast<float> (viewportRect_.getY()) + h * 0.5f;
    const float halfUsable = juce::jmin (w, h) * 0.5f - kEdgeInset;

    if (halfUsable <= 0.0f)
        return;

    const int numPoints = juce::jlimit (0, mdsp_ui::scopes::StereoScopeRenderState::kMaxPoints, state_.numPoints);
    if (numPoints > 0)
    {
        cachedLivePath_.preallocateSpace (juce::jmax (8, numPoints * 3));
        bool first = true;
        for (int i = 0; i < numPoints; ++i)
        {
            const auto p = state_.points[static_cast<size_t> (i)];
            if (!std::isfinite (p.x) || !std::isfinite (p.y))
                continue;

            const juce::Point<float> pt { cx + p.x * halfUsable, cy - p.y * halfUsable };
            if (first)
            {
                cachedLivePath_.startNewSubPath (pt);
                first = false;
            }
            else
            {
                cachedLivePath_.lineTo (pt);
            }
        }
    }

    const int activeFrames = juce::jlimit (0,
                                           mdsp_ui::scopes::StereoScopeRenderState::kHistoryFrames,
                                           state_.activeHistoryFrames);
    const int pointsPerFrame = juce::jlimit (1,
                                             mdsp_ui::scopes::StereoScopeRenderState::kPointsPerHistoryFrame,
                                             state_.pointsPerHistoryFrame);
    const int newestFrame = juce::jlimit (-1,
                                          mdsp_ui::scopes::StereoScopeRenderState::kHistoryFrames - 1,
                                          state_.newestHistoryFrame);

    if (activeFrames > 0 && newestFrame >= 0)
    {
        for (int age = 0; age < activeFrames; ++age)
        {
            const int frameIndex = (newestFrame - age + mdsp_ui::scopes::StereoScopeRenderState::kHistoryFrames)
                                   % mdsp_ui::scopes::StereoScopeRenderState::kHistoryFrames;
            const int base = frameIndex * pointsPerFrame;
            auto& path = cachedHistoryPaths_[static_cast<size_t> (age)];
            path.preallocateSpace (juce::jmax (8, pointsPerFrame * 3));

            bool first = true;
            for (int i = 0; i < pointsPerFrame; ++i)
            {
                const auto p = state_.historyPoints[static_cast<size_t> (base + i)];
                if (! std::isfinite (p.x) || ! std::isfinite (p.y))
                    continue;

                const juce::Point<float> pt { cx + p.x * halfUsable, cy - p.y * halfUsable };
                if (first)
                {
                    path.startNewSubPath (pt);
                    first = false;
                }
                else
                {
                    path.lineTo (pt);
                }
            }
        }
    }

    if (state_.holdEnabled && state_.holdPointCount > 0)
    {
        bool holdFirst = true;
        const int holdCount = juce::jlimit (0,
                                            mdsp_ui::scopes::StereoScopeRenderState::kHoldBins,
                                            state_.holdPointCount);
        cachedHoldPath_.preallocateSpace (juce::jmax (8, holdCount * 3));

        for (int i = 0; i < holdCount; ++i)
        {
            const auto p = state_.holdPoints[static_cast<size_t> (i)];
            if (!std::isfinite (p.x) || !std::isfinite (p.y))
                continue;

            const juce::Point<float> pt { cx + p.x * halfUsable, cy - p.y * halfUsable };
            if (holdFirst)
            {
                cachedHoldPath_.startNewSubPath (pt);
                holdFirst = false;
            }
            else
            {
                cachedHoldPath_.lineTo (pt);
            }
        }

        if (! holdFirst)
            cachedHoldPath_.closeSubPath();
    }
}

void StereoScopeComponent::paint (juce::Graphics& g)
{
    const auto& theme = ui_.theme();
    g.fillAll (theme.panel);

    if (viewportRect_.isEmpty())
        return;

    const auto plot = viewportRect_.toFloat();
    const float cx = plot.getCentreX();
    const float cy = plot.getCentreY();

    g.setColour (theme.grid);
    g.drawVerticalLine (static_cast<int> (cx), plot.getY(), plot.getBottom());
    g.drawHorizontalLine (static_cast<int> (cy), plot.getX(), plot.getRight());

    if (enabled_)
    {
        const auto scopeColour = traceColourOrFallback (traceColors_, AnalyzerPro::TraceId::Peak, theme.seriesPeak);
        g.saveState();
        g.reduceClipRegion (viewportRect_);

        const int activeFrames = juce::jlimit (0,
                                               mdsp_ui::scopes::StereoScopeRenderState::kHistoryFrames,
                                               state_.activeHistoryFrames);
        for (int age = activeFrames - 1; age >= 0; --age)
        {
            const auto& path = cachedHistoryPaths_[static_cast<size_t> (age)];
            if (path.isEmpty())
                continue;

            const float newestness = (activeFrames <= 1)
                                         ? 1.0f
                                         : 1.0f - (static_cast<float> (age) / static_cast<float> (activeFrames - 1));
            const float alpha = juce::jmap (newestness, 0.08f, 0.62f);
            g.setColour (scopeColour.withAlpha (alpha));
            g.strokePath (path, juce::PathStrokeType (1.05f));
        }

        if (! cachedLivePath_.isEmpty())
        {
            g.setColour (scopeColour.withAlpha (0.82f));
            g.strokePath (cachedLivePath_, juce::PathStrokeType (1.25f));
        }

        if (state_.holdEnabled && ! cachedHoldPath_.isEmpty())
        {
            g.setColour (scopeColour.withAlpha (0.45f));
            g.strokePath (cachedHoldPath_, juce::PathStrokeType (1.8f));
        }

        g.restoreState();
    }

    g.setColour (theme.borderDivider);
    g.drawRect (getLocalBounds().toFloat(), 1.0f);
}
