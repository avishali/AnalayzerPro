#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <mdsp_ui/UiContext.h>
#include "../../../control/ControlBinder.h"
#include "../../../control/ControlIds.h"
#include <functional>

class PeakControlsRow : public juce::Component
{
public:
    explicit PeakControlsRow (mdsp_ui::UiContext& ui);

    void setControlBinder (AnalyzerPro::ControlBinder& binder);
    void setResetCallback (std::function<void()> cb);
    int getPreferredHeight() const noexcept;
    void layout (juce::Rectangle<int> bounds, int& y);

    void resized() override;

private:
    mdsp_ui::UiContext& ui_;
    AnalyzerPro::ControlBinder* controlBinder = nullptr;
    std::function<void()> onReset_;

    juce::Label peakHoldLabel_;
    juce::Label holdLabel_;
    juce::Label decayLabel_;

    juce::ToggleButton peakHoldToggle_;
    juce::ToggleButton holdToggle_;
    juce::TextButton resetButton_ { "Reset" };
    juce::Slider decaySlider_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PeakControlsRow)
};

inline PeakControlsRow::PeakControlsRow (mdsp_ui::UiContext& ui)
    : ui_ (ui)
{
    const auto& theme = ui_.theme();
    const auto& type = ui_.type();
    const auto& m = ui_.metrics();

    peakHoldLabel_.setText ("Peak Hold", juce::dontSendNotification);
    peakHoldLabel_.setFont (type.labelSmallFont());
    peakHoldLabel_.setJustificationType (juce::Justification::centredLeft);
    peakHoldLabel_.setColour (juce::Label::textColourId, theme.grey);
    addAndMakeVisible (peakHoldLabel_);

    holdLabel_.setText ("Hold", juce::dontSendNotification);
    holdLabel_.setFont (type.labelSmallFont());
    holdLabel_.setJustificationType (juce::Justification::centredLeft);
    holdLabel_.setColour (juce::Label::textColourId, theme.grey);
    addAndMakeVisible (holdLabel_);

    decayLabel_.setText ("Peak Decay", juce::dontSendNotification);
    decayLabel_.setFont (type.labelSmallFont());
    decayLabel_.setJustificationType (juce::Justification::centredLeft);
    decayLabel_.setColour (juce::Label::textColourId, theme.grey);
    addAndMakeVisible (decayLabel_);

    peakHoldToggle_.setButtonText ("On");
    peakHoldToggle_.setToggleState (true, juce::dontSendNotification);
    addAndMakeVisible (peakHoldToggle_);

    holdToggle_.setButtonText ("Hold");
    addAndMakeVisible (holdToggle_);

    resetButton_.setTooltip ("Clear peak trace");
    resetButton_.onClick = [this]
    {
        if (onReset_)
            onReset_();
    };
    addAndMakeVisible (resetButton_);

    decaySlider_.setSliderStyle (juce::Slider::LinearHorizontal);
    decaySlider_.setTextBoxStyle (juce::Slider::TextBoxRight, false, m.sliderTextBoxW, m.sliderTextBoxH);
    decaySlider_.setRange (0.0, 10.0, 0.1);
    decaySlider_.setValue (1.0, juce::dontSendNotification);
    addAndMakeVisible (decaySlider_);
}

inline void PeakControlsRow::setControlBinder (AnalyzerPro::ControlBinder& binder)
{
    controlBinder = &binder;

    if (controlBinder != nullptr)
    {
        controlBinder->bindToggle (AnalyzerPro::ControlId::AnalyzerPeakHold, peakHoldToggle_);
        controlBinder->bindToggle (AnalyzerPro::ControlId::AnalyzerHold, holdToggle_);
        controlBinder->bindSlider (AnalyzerPro::ControlId::AnalyzerPeakDecay, decaySlider_);
    }
}

inline void PeakControlsRow::setResetCallback (std::function<void()> cb)
{
    onReset_ = std::move (cb);
}

inline int PeakControlsRow::getPreferredHeight() const noexcept
{
    const auto& m = ui_.metrics();
    const int controlH = juce::jmax (m.buttonSmallH, m.sliderH);
    return m.secondaryHeight + controlH + m.gapSmall;
}

inline void PeakControlsRow::layout (juce::Rectangle<int> bounds, int& y)
{
    const auto& m = ui_.metrics();
    const int controlH = juce::jmax (m.buttonSmallH, m.sliderH);
    bounds.setHeight (m.secondaryHeight + controlH);
    setBounds (bounds);
    y += m.secondaryHeight + controlH + m.gapSmall;
}

inline void PeakControlsRow::resized()
{
    const auto& m = ui_.metrics();
    const int labelH = m.secondaryHeight;
    const int controlH = juce::jmax (m.buttonSmallH, m.sliderH);
    const int toggleW = m.buttonSmallW;
    const int resetW = m.buttonW;
    const int gap = m.gapSmall;
    int clusterGap = m.gap;

    auto area = getLocalBounds();
    auto labelArea = area.removeFromTop (labelH);
    auto controlArea = area;

    const auto labelFont = peakHoldLabel_.getFont();
    const int peakLabelW = juce::jmax (m.headerModeLabelW, static_cast<int> (labelFont.getStringWidthFloat ("Peak Hold")) + 6);
    const int holdLabelW = juce::jmax (m.headerModeLabelW / 2, static_cast<int> (labelFont.getStringWidthFloat ("Hold")) + 6);

    const int leftW = peakLabelW + gap + toggleW + gap
                    + holdLabelW + gap + toggleW + gap
                    + resetW;
    int available = controlArea.getWidth() - leftW - clusterGap;
    int sliderW = juce::jmax (0, available);
    if (sliderW < 160)
    {
        clusterGap = m.gapSmall;
        available = controlArea.getWidth() - leftW - clusterGap;
        sliderW = juce::jmax (0, available);
    }

    const int controlY = controlArea.getY() + (controlH - m.buttonSmallH) / 2;
    const int sliderY = controlArea.getY() + (controlH - m.sliderH) / 2;

    int x = controlArea.getX();

    peakHoldLabel_.setBounds (x, labelArea.getY(), peakLabelW, labelH);
    peakHoldToggle_.setBounds (x + peakLabelW + gap, controlY, toggleW, m.buttonSmallH);
    x += peakLabelW + gap + toggleW + gap;

    holdLabel_.setBounds (x, labelArea.getY(), holdLabelW, labelH);
    holdToggle_.setBounds (x + holdLabelW + gap, controlY, toggleW, m.buttonSmallH);
    x += holdLabelW + gap + toggleW + gap;

    resetButton_.setBounds (x, controlY, resetW, m.buttonSmallH);
    x += resetW + clusterGap;

    decayLabel_.setBounds (x, labelArea.getY(), sliderW, labelH);
    decaySlider_.setBounds (x, sliderY, sliderW, m.sliderH);
}
