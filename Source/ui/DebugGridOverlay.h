#pragma once

#if JUCE_DEBUG

#include <cstdio>
#include <vector>
#include <juce_gui_basics/juce_gui_basics.h>

struct DebugRectEntry
{
    juce::String name;
    juce::Rectangle<int> rect;
    juce::Colour colour;
};

class DebugGridOverlay final : public juce::Component,
                               private juce::Timer
{
public:
    DebugGridOverlay()
    {
        setInterceptsMouseClicks (false, false);
    }

    void setEnabled (bool v) { enabled_ = v; updateMouseTimer(); }
    bool isEnabled() const { return enabled_; }

    void setStepPx (int v) { stepPx_ = juce::jmax (2, v); }
    int getStepPx() const { return stepPx_; }

    void setMajorEvery (int v) { majorEvery_ = juce::jmax (1, v); }
    int getMajorEvery() const { return majorEvery_; }

    void setShowLabels (bool v) { showLabels_ = v; }
    bool getShowLabels() const { return showLabels_; }
    void setShowOriginCross (bool v) { showOriginCross_ = v; }
    void setShowRulers (bool v) { showRulers_ = v; }
    bool getShowRulers() const { return showRulers_; }
    void setShowOuterBounds (bool v) { showOuterBounds_ = v; }
    bool getShowOuterBounds() const { return showOuterBounds_; }
    void setShowPanelHighlights (bool v) { showPanelHighlights_ = v; }
    bool getShowPanelHighlights() const { return showPanelHighlights_; }

    void clearDebugRects() { debugRects_.clear(); }
    void addDebugRect (const juce::String& name, juce::Rectangle<int> rect, juce::Colour colour)
    {
        debugRects_.push_back ({ name, rect, colour });
    }

    void updateMousePosition (juce::Point<int> pos)
    {
        if (mousePos_ != pos)
        {
            mousePos_ = pos;
            repaint();
        }
    }

    void paint (juce::Graphics& g) override
    {
        if (!enabled_)
            return;

        const int w = getWidth();
        const int h = getHeight();
        const float majorAlpha = 0.18f;
        const float minorAlpha = 0.08f;
        const float originAlpha = 0.35f;
        const float frameAlpha = 0.6f;
        const float rulerAlpha = 0.12f;
        const int rulerHeight = 20;
        const int rulerWidth = 20;

        if (showOuterBounds_)
        {
            g.setColour (juce::Colours::red.withAlpha (frameAlpha));
            g.drawRect (0, 0, w, h, 1);
            g.setFont (juce::FontOptions (11.0f));
            char buf[48];
            snprintf (buf, sizeof (buf), "Editor: %dx%d", w, h);
            g.setColour (juce::Colours::red.withAlpha (frameAlpha));
            g.drawSingleLineText (buf, 4, 12);
        }

        if (showRulers_)
        {
            g.setColour (juce::Colours::white.withAlpha (rulerAlpha));
            g.fillRect (0, 0, w, rulerHeight);
            g.fillRect (0, 0, rulerWidth, h);
            g.setColour (juce::Colours::white.withAlpha (0.25f));
            for (int x = 0; x <= w; x += stepPx_)
            {
                const int stepIndex = x / stepPx_;
                const bool isMajor = (stepIndex % majorEvery_) == 0;
                const int tickLen = isMajor ? 8 : 4;
                g.drawVerticalLine (x, rulerHeight - tickLen, rulerHeight + 0.0f);
                if (isMajor && x < w - 24)
                {
                    char buf[16];
                    snprintf (buf, sizeof (buf), "%d", x);
                    g.setFont (juce::FontOptions (9.0f));
                    g.drawSingleLineText (buf, x + 2, 10);
                }
            }
            for (int y = 0; y <= h; y += stepPx_)
            {
                const int stepIndex = y / stepPx_;
                const bool isMajor = (stepIndex % majorEvery_) == 0;
                const int tickLen = isMajor ? 8 : 4;
                g.drawHorizontalLine (y, rulerWidth - tickLen, rulerWidth + 0.0f);
                if (isMajor && y < h - 12)
                {
                    char buf[16];
                    snprintf (buf, sizeof (buf), "%d", y);
                    g.setFont (juce::FontOptions (9.0f));
                    g.drawSingleLineText (buf, 2, y + 10);
                }
            }
        }

        if (showOriginCross_)
        {
            g.setColour (juce::Colours::lime.withAlpha (originAlpha));
            g.drawVerticalLine (0, 0.0f, static_cast<float> (h));
            g.drawHorizontalLine (0, 0.0f, static_cast<float> (w));
        }

        for (int x = 0; x <= w; x += stepPx_)
        {
            const int stepIndex = x / stepPx_;
            const bool isMajor = (stepIndex % majorEvery_) == 0;
            g.setColour (juce::Colours::white.withAlpha (isMajor ? majorAlpha : minorAlpha));
            g.drawVerticalLine (x, 0.0f, static_cast<float> (h));
        }

        for (int y = 0; y <= h; y += stepPx_)
        {
            const int stepIndex = y / stepPx_;
            const bool isMajor = (stepIndex % majorEvery_) == 0;
            g.setColour (juce::Colours::white.withAlpha (isMajor ? majorAlpha : minorAlpha));
            g.drawHorizontalLine (y, 0.0f, static_cast<float> (w));
        }

        if (showLabels_)
        {
            g.setFont (juce::FontOptions (11.0f));
            char buf[32];
            for (int x = 0; x <= w; x += stepPx_)
            {
                const int stepIndexX = x / stepPx_;
                if ((stepIndexX % majorEvery_) != 0)
                    continue;
                for (int y = 0; y <= h; y += stepPx_)
                {
                    const int stepIndexY = y / stepPx_;
                    if ((stepIndexY % majorEvery_) != 0)
                        continue;
                    snprintf (buf, sizeof (buf), "%d,%d", x, y);
                    g.setColour (juce::Colours::white.withAlpha (majorAlpha));
                    g.drawSingleLineText (buf, x + 2, y + 12);
                }
            }
        }

        if (showPanelHighlights_)
        {
            const float dashLengths[] = { 4.0f, 4.0f };
            for (const auto& e : debugRects_)
            {
                g.setColour (e.colour.withAlpha (0.6f));
                const auto r = e.rect.toFloat();
                g.drawDashedLine (juce::Line<float> (r.getX(), r.getY(), r.getRight(), r.getY()), dashLengths, 2, 1.0f);
                g.drawDashedLine (juce::Line<float> (r.getRight(), r.getY(), r.getRight(), r.getBottom()), dashLengths, 2, 1.0f);
                g.drawDashedLine (juce::Line<float> (r.getRight(), r.getBottom(), r.getX(), r.getBottom()), dashLengths, 2, 1.0f);
                g.drawDashedLine (juce::Line<float> (r.getX(), r.getBottom(), r.getX(), r.getY()), dashLengths, 2, 1.0f);
                g.setFont (juce::FontOptions (10.0f));
                g.drawSingleLineText (e.name, e.rect.getX() + 2, e.rect.getY() + 12);
            }
        }

        if (mousePos_.x >= 0 && mousePos_.y >= 0)
        {
            g.setFont (juce::FontOptions (11.0f));
            g.setColour (juce::Colours::white.withAlpha (0.8f));
            char buf[32];
            snprintf (buf, sizeof (buf), "x: %d  y: %d", mousePos_.x, mousePos_.y);
            g.drawSingleLineText (buf, 4, h - 6);
        }
    }

private:
    void timerCallback() override
    {
        if (!enabled_ || getParentComponent() == nullptr)
            return;
        auto screenPos = juce::Desktop::getInstance().getMousePosition();
        auto localPos = getParentComponent()->getLocalPoint (nullptr, screenPos);
        updateMousePosition (localPos);
    }

    void updateMouseTimer()
    {
        if (enabled_)
            startTimerHz (20);
        else
            stopTimer();
    }

    bool enabled_ = false;
    int stepPx_ = 8;
    int majorEvery_ = 4;
    bool showLabels_ = true;
    bool showOriginCross_ = true;
    bool showRulers_ = true;
    bool showOuterBounds_ = true;
    bool showPanelHighlights_ = false;
    juce::Point<int> mousePos_ { -1, -1 };
    std::vector<DebugRectEntry> debugRects_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DebugGridOverlay)
};

#endif
