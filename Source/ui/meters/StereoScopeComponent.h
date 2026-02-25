#pragma once

#include <array>

#include <juce_gui_basics/juce_gui_basics.h>
#include <mdsp_ui/UiContext.h>
#include <mdsp_ui/scopes/StereoScopeRenderState.h>

class StereoScopeComponent : public juce::Component
{
public:
    using ScopeActionFn = void (*) (void*) noexcept;

    static constexpr int kDefaultMaxViewportSize = 360;

    explicit StereoScopeComponent (mdsp_ui::UiContext& ui);
    ~StereoScopeComponent() override = default;

    void setEnabled (bool enabled) noexcept;
    bool isEnabled() const noexcept { return enabled_; }

    void setMaxViewportSize (int maxSize) noexcept;

    void setRenderState (const mdsp_ui::scopes::StereoScopeRenderState& state);
    const mdsp_ui::scopes::StereoScopeRenderState& getRenderState() const noexcept { return state_; }

    float getCorrelation() const noexcept { return state_.correlation; }

    void setFreezeToggleCallback (ScopeActionFn fn, void* ctx) noexcept;
    void setResetCallback (ScopeActionFn fn, void* ctx) noexcept;
    void triggerFreezeToggle() noexcept;
    void triggerReset() noexcept;

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    void rebuildCachedPaths();

    mdsp_ui::UiContext& ui_;
    mdsp_ui::scopes::StereoScopeRenderState state_ {};
    std::array<juce::Path, mdsp_ui::scopes::StereoScopeRenderState::kHistoryFrames> cachedHistoryPaths_ {};
    juce::Path cachedLivePath_;
    juce::Path cachedHoldPath_;
    juce::Rectangle<int> viewportRect_;
    int maxViewportSize_ = kDefaultMaxViewportSize;
    bool enabled_ = true;

    ScopeActionFn onFreezeToggle_ = nullptr;
    void* freezeCtx_ = nullptr;
    ScopeActionFn onReset_ = nullptr;
    void* resetCtx_ = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StereoScopeComponent)
};
