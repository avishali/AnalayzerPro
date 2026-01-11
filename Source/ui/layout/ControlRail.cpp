#include "ControlRail.h"
#include "../../control/ControlIds.h"

//==============================================================================
ControlRail::ControlRail (mdsp_ui::UiContext& ui)
    : ui_ (ui),
      navigateHeader (ui, "Navigate"),
      analyzerHeader (ui, "Analyzer"),
      displayHeader (ui, "Display"),
      metersHeader (ui, "Meters"),
      dbRangeRow (ui, "dB Range", dbRangeCombo),
      displayGainRow (ui, "Display Gain", displayGainSlider, -24.0, 24.0, 0.5, 0.0),
      tiltRow (ui, "Tilt", tiltCombo),
      gainRow (ui, "Gain", gainSlider, 0.0, 2.0, 0.01, 1.0),
      peakControlsRow_ (ui)
{
    const auto& theme = ui_.theme();
    const auto& type = ui_.type();
    // Attach section headers to parent
    navigateHeader.attachToParent (*this);
    analyzerHeader.attachToParent (*this);
    displayHeader.attachToParent (*this);
    metersHeader.attachToParent (*this);

    // Attach control rows to parent
    dbRangeRow.attachToParent (*this);
    displayGainRow.attachToParent (*this);
    tiltRow.attachToParent (*this);
    gainRow.attachToParent (*this);
    addAndMakeVisible (peakControlsRow_);

    // Configure combos (items and defaults)
    dbRangeCombo.addItem ("-60 dB", 1);
    dbRangeCombo.addItem ("-90 dB", 2);
    dbRangeCombo.addItem ("-120 dB", 3);
    dbRangeCombo.setSelectedId (3, juce::dontSendNotification);  // Default: -120 dB
    dbRangeCombo.onChange = [this]
    {
        triggerDbRangeChanged();
    };
    
    tiltCombo.addItem ("Flat", 1);
    tiltCombo.addItem ("Pink", 2);
    tiltCombo.addItem ("White", 3);
    tiltCombo.setSelectedId (1, juce::dontSendNotification);  // Default: Flat

    gainSlider.onValueChange = [this]
    {
        triggerGainChanged();
    };

    // Placeholder labels (smaller, dimmer)
    placeholderLabel1.setText ("Controls...", juce::dontSendNotification);
    placeholderLabel1.setFont (type.placeholderFont());
    placeholderLabel1.setJustificationType (juce::Justification::centredLeft);
    placeholderLabel1.setColour (juce::Label::textColourId, theme.grey);
    addAndMakeVisible (placeholderLabel1);

    placeholderLabel3.setText ("Controls...", juce::dontSendNotification);
    placeholderLabel3.setFont (type.placeholderFont());
    placeholderLabel3.setJustificationType (juce::Justification::centredLeft);
    placeholderLabel3.setColour (juce::Label::textColourId, theme.grey);
    addAndMakeVisible (placeholderLabel3);
}

ControlRail::~ControlRail() = default;

void ControlRail::setControlBinder (AnalyzerPro::ControlBinder& binder)
{
    controlBinder = &binder;
    
    // Bind controls now that binder is available
    if (controlBinder != nullptr)
    {
        peakControlsRow_.setControlBinder (*controlBinder);
        controlBinder->bindSlider (AnalyzerPro::ControlId::AnalyzerDisplayGain, displayGainSlider);
        controlBinder->bindCombo (AnalyzerPro::ControlId::AnalyzerTilt, tiltCombo);
    }
}

void ControlRail::setResetPeaksCallback (std::function<void()> cb)
{
    peakControlsRow_.setResetCallback (std::move (cb));
}

void ControlRail::setDbRangeChangedCallback (std::function<void (int)> cb)
{
    onDbRangeChanged_ = std::move (cb);
}

void ControlRail::setGainChangedCallback (std::function<void (float)> cb)
{
    onGainChanged_ = std::move (cb);
}

void ControlRail::setGainValue (float gain)
{
    gainSlider.setValue (gain, juce::dontSendNotification);
}

void ControlRail::triggerDbRangeChanged()
{
    if (onDbRangeChanged_ != nullptr)
        onDbRangeChanged_ (dbRangeCombo.getSelectedId());
}

void ControlRail::triggerGainChanged()
{
    if (onGainChanged_ != nullptr)
        onGainChanged_ (static_cast<float> (gainSlider.getValue()));
}

void ControlRail::paint (juce::Graphics& g)
{
    const auto& theme = ui_.theme();

    // Dark panel background with subtle contrast
    g.fillAll (theme.panel);
    g.setColour (theme.borderDivider);
    g.fillRect (getLocalBounds().removeFromLeft (1));
}

int ControlRail::getPreferredHeight() const noexcept
{
    const auto& m = ui_.metrics();

    int height = 0;

    // Section 1: Navigate
    height += m.titleHeight + m.titleSecondaryGap;
    height += m.secondaryHeight;
    height += m.sectionSpacing;

    // Section 2: Analyzer
    height += m.titleHeight + m.titleSecondaryGap;
    height += m.secondaryHeight + m.comboH + m.gapSmall;      // dB Range
    height += peakControlsRow_.getPreferredHeight();
    height += m.secondaryHeight + m.sliderH + m.gapSmall;      // Display Gain
    height += m.sectionSpacing;

    // Section 3: Display
    height += m.titleHeight + m.titleSecondaryGap;
    height += m.secondaryHeight + m.comboH + m.gapSmall;       // Tilt
    height += m.sectionSpacing - m.gapSmall;

    // Section 4: Meters
    height += m.titleHeight + m.titleSecondaryGap;
    height += m.secondaryHeight + m.sliderH + m.gapSmall; // Gain

    // Add top/bottom padding from resized()
    height += m.padSmall * 2;

    return height;
}

void ControlRail::resized()
{
    const auto& m = ui_.metrics();
    auto bounds = getLocalBounds().reduced (m.padSmall);  // Reduced outer padding
    int y = bounds.getY();

    // Section 1: Navigate
    navigateHeader.layout (bounds, y);
    placeholderLabel1.setBounds (bounds.getX(), y, bounds.getWidth(), m.secondaryHeight);
    y += m.secondaryHeight + m.sectionSpacing;

    // Section 2: Analyzer
    analyzerHeader.layout (bounds, y);
    
    dbRangeRow.layout (bounds, y);
    peakControlsRow_.layout (bounds, y);
    displayGainRow.layout (bounds, y);
    y += m.sectionSpacing;

    // Section 3: Display
    displayHeader.layout (bounds, y);
    tiltRow.layout (bounds, y);
    // TiltRow already added gapSmall, so we only need to add sectionSpacing - gapSmall for proper spacing
    y += m.sectionSpacing - m.gapSmall;

    // Section 4: Meters (placeholder)
    metersHeader.layout (bounds, y);
    gainRow.layout (bounds, y);
}
