#include "DraggableParamValueLabel.h"

namespace AnalyzerPro
{

DraggableParamValueLabel::DraggableParamValueLabel (mdsp_ui::UiContext& ui)
    : ui_ (ui)
{
    setMouseCursor (juce::MouseCursor::NormalCursor);
    setWantsMouseWheelEvents (true);
}

DraggableParamValueLabel::~DraggableParamValueLabel()
{
    stopTimer();
}

void DraggableParamValueLabel::setParameter (juce::RangedAudioParameter* param)
{
    param_ = param;
    if (param_ != nullptr)
    {
        repaint();
        startTimerHz (15);
    }
    else
    {
        stopTimer();
    }
}

void DraggableParamValueLabel::timerCallback()
{
    if (param_ != nullptr && ! dragging_)
        repaint();
}

void DraggableParamValueLabel::paint (juce::Graphics& g)
{
    const auto& theme = ui_.theme();
    const auto& type = ui_.type();

    double value = 0.0;
    if (param_ != nullptr)
        value = param_->getNormalisableRange().convertFrom0to1 (param_->getValue());

    juce::String text = formatValue (value);
    g.setFont (type.labelSmallFont());
    g.setColour (hover_ ? theme.lightGrey : theme.grey);
    g.drawText (text, getLocalBounds(), juce::Justification::centredLeft, true);
}

void DraggableParamValueLabel::mouseEnter (const juce::MouseEvent&)
{
    hover_ = true;
    setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
    repaint();
}

void DraggableParamValueLabel::mouseExit (const juce::MouseEvent&)
{
    hover_ = false;
    setMouseCursor (juce::MouseCursor::NormalCursor);
    repaint();
}

void DraggableParamValueLabel::mouseDown (const juce::MouseEvent& e)
{
    if (param_ == nullptr)
        return;

    param_->beginChangeGesture();
    dragging_ = true;
    startValue_ = param_->getNormalisableRange().convertFrom0to1 (param_->getValue());
    startY_ = e.getPosition().getY();
}

void DraggableParamValueLabel::mouseDrag (const juce::MouseEvent& e)
{
    if (param_ == nullptr || ! dragging_)
        return;

    const int deltaY = startY_ - e.getPosition().getY(); // Up = positive = increase
    const bool fine = e.mods.isShiftDown();
    const bool coarse = e.mods.isCommandDown();
    const double sens = getSensitivity (fine, coarse);

    const auto& range = param_->getNormalisableRange();
    double newValue = startValue_ + deltaY * sens;
    newValue = juce::jlimit (static_cast<double> (range.start), static_cast<double> (range.end), newValue);

    const float norm = range.convertTo0to1 (static_cast<float> (newValue));
    param_->setValueNotifyingHost (norm);

    repaint();
}

void DraggableParamValueLabel::mouseUp (const juce::MouseEvent&)
{
    if (param_ != nullptr && dragging_)
    {
        param_->endChangeGesture();
        dragging_ = false;
    }
    repaint();
}

void DraggableParamValueLabel::mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& wheel)
{
    if (param_ == nullptr)
        return;

    const bool fine = juce::ModifierKeys::getCurrentModifiers().isShiftDown();
    const bool coarse = juce::ModifierKeys::getCurrentModifiers().isCommandDown();
    const double sens = getSensitivity (fine, coarse);

    const auto& range = param_->getNormalisableRange();
    double value = range.convertFrom0to1 (param_->getValue());
    const float delta = static_cast<float> (-wheel.deltaY * sens * 5.0);
    value = juce::jlimit (static_cast<double> (range.start), static_cast<double> (range.end), value + delta);

    param_->beginChangeGesture();
    param_->setValueNotifyingHost (range.convertTo0to1 (static_cast<float> (value)));
    param_->endChangeGesture();

    repaint();
}

juce::String DraggableParamValueLabel::formatValue (double value) const
{
    if (value >= 1000.0)
        return juce::String (value / 1000.0, 1) + " s";
    return juce::String (juce::roundToInt (value)) + suffix_;
}

double DraggableParamValueLabel::getSensitivity (bool fine, bool coarse) const
{
    if (param_ == nullptr)
        return 1.0;

    const auto& range = param_->getNormalisableRange();
    const double fullRange = range.end - range.start;
    double step = fullRange / static_cast<double> (kPixelsForFullRange);

    if (fine)
        step *= kFineMultiplier;
    else if (coarse)
        step *= kCoarseMultiplier;

    return step;
}

} // namespace AnalyzerPro
