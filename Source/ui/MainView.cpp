#include "MainView.h"

static constexpr ui_core::ControlId kGainControlId = 1001;
static constexpr ui_core::ControlId kOutputControlId = 1002;

//==============================================================================
MainView::MainView (PluginTemplateAudioProcessor& p)
    : audioProcessor (p)
{
    setWantsKeyboardFocus (true);

    // Gain Label
    gainLabel.setText ("Gain", juce::dontSendNotification);
    gainLabel.attachToComponent (&gainSlider, false);
    gainLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (gainLabel);

    // Gain Slider
    gainSlider.setSliderStyle (juce::Slider::RotaryVerticalDrag);
    gainSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 20);
    gainSlider.setRange (0.0, 2.0, 0.01);
    gainSlider.setValue (audioProcessor.getParameters().getGain());
    gainSlider.onValueChange = [this] { gainSliderChanged(); };
    gainSlider.onDragStart = [this]
    {
        const auto prev = focusManager.getFocusedControl();
        focusManager.setFocusedControl (kGainControlId);
        audioProcessor.getParameters().setFocusedControlId (static_cast<int> (kGainControlId));
        if (hardwareOutput)
        {
            if (prev && *prev != kGainControlId)
                hardwareOutput->setFocus (*prev, false);
            hardwareOutput->setFocus (kGainControlId, true);
        }
    };
    addAndMakeVisible (gainSlider);

    // Output Label
    outputLabel.setText ("Output", juce::dontSendNotification);
    outputLabel.attachToComponent (&outputSlider, false);
    outputLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (outputLabel);

    // Output Slider
    outputSlider.setSliderStyle (juce::Slider::RotaryVerticalDrag);
    outputSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 20);
    outputSlider.setRange (0.0, 2.0, 0.01);
    outputSlider.setValue (audioProcessor.getParameters().getOutputGain());
    outputSlider.onValueChange = [this] { outputSliderChanged(); };
    outputSlider.onDragStart = [this]
    {
        const auto prev = focusManager.getFocusedControl();
        focusManager.setFocusedControl (kOutputControlId);
        audioProcessor.getParameters().setFocusedControlId (static_cast<int> (kOutputControlId));
        if (hardwareOutput)
        {
            if (prev && *prev != kOutputControlId)
                hardwareOutput->setFocus (*prev, false);
            hardwareOutput->setFocus (kOutputControlId, true);
        }
    };
    addAndMakeVisible (outputSlider);

    // Setup focus adapters
    gainFocusAdapter.controlId = kGainControlId;
    gainFocusAdapter.focusedControlIdPtr = &focusedControlId;
    gainFocusAdapter.repaintTarget = this;
    focusManager.registerWidget (kGainControlId, &gainFocusAdapter);

    outputFocusAdapter.controlId = kOutputControlId;
    outputFocusAdapter.focusedControlIdPtr = &focusedControlId;
    outputFocusAdapter.repaintTarget = this;
    focusManager.registerWidget (kOutputControlId, &outputFocusAdapter);

    // Add binding to registry (mapped: native 0..2, normalized 0..1)
    bindingRegistry.add (ui_core::makeMappedBinding (
        kGainControlId,
        [this]() { return audioProcessor.getParameters().getGain(); },
        [this](float native)
        {
            audioProcessor.getParameters().setGain (native);
            // keep UI in sync without recursion
            gainSlider.setValue (native, juce::dontSendNotification);
            // Send LED feedback (convert native to normalized)
            if (hardwareOutput)
                hardwareOutput->setLEDValue (kGainControlId, native / 2.0f);
        },
        [](float normalized) { return normalized * 2.0f; },      // toNative: 0..1 -> 0..2
        [](float native) { return native / 2.0f; }));            // toNormalized: 0..2 -> 0..1

    bindingRegistry.add (ui_core::makeMappedBinding (
        kOutputControlId,
        [this]() { return audioProcessor.getParameters().getOutputGain(); },
        [this](float native)
        {
            audioProcessor.getParameters().setOutputGain (native);
            // keep UI in sync without recursion
            outputSlider.setValue (native, juce::dontSendNotification);
            // Send LED feedback (convert native to normalized)
            if (hardwareOutput)
                hardwareOutput->setLEDValue (kOutputControlId, native / 2.0f);
        },
        [](float normalized) { return normalized * 2.0f; },      // toNative: 0..1 -> 0..2
        [](float native) { return native / 2.0f; }));            // toNormalized: 0..2 -> 0..1

    // Create hardware adapter
    hardwareAdapter = std::make_unique<PluginHardwareAdapter> (bindingRegistry);
    
    // Create hardware output adapter
    hardwareOutput = std::make_unique<PluginHardwareOutputAdapter>();

    // Restore persisted focus
    int persistedId = audioProcessor.getParameters().getFocusedControlId();
    ui_core::ControlId focusIdToRestore = kGainControlId; // default fallback
    
    // Validate: check if binding exists for persisted ID
    if (bindingRegistry.find (static_cast<ui_core::ControlId> (persistedId)) != nullptr)
        focusIdToRestore = static_cast<ui_core::ControlId> (persistedId);
    
    focusManager.setFocusedControl (focusIdToRestore);
    if (hardwareOutput)
    {
        // Clear other controls
        if (focusIdToRestore != kGainControlId)
            hardwareOutput->setFocus (kGainControlId, false);
        if (focusIdToRestore != kOutputControlId)
            hardwareOutput->setFocus (kOutputControlId, false);
        // Set focused control
        hardwareOutput->setFocus (focusIdToRestore, true);
    }

    setSize (400, 500);
}

MainView::~MainView()
{
    focusManager.unregisterWidget (kGainControlId, &gainFocusAdapter);
    focusManager.unregisterWidget (kOutputControlId, &outputFocusAdapter);
}

void MainView::paint (juce::Graphics& g)
{
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));

    if (focusedControlId == kGainControlId)
    {
        auto focusBounds = gainSlider.getBounds().expanded (4);
        g.setColour (juce::Colours::white.withAlpha (0.25f));
        g.drawRoundedRectangle (focusBounds.toFloat(), 12.0f, 2.0f);
    }
    else if (focusedControlId == kOutputControlId)
    {
        auto focusBounds = outputSlider.getBounds().expanded (4);
        g.setColour (juce::Colours::white.withAlpha (0.25f));
        g.drawRoundedRectangle (focusBounds.toFloat(), 12.0f, 2.0f);
    }
}

// REPLACE — MainView::resized() (Source/ui/MainView.cpp)
void MainView::resized()
{
    auto area = getLocalBounds().reduced (20);

    const int rowHeight   = area.getHeight() / 2;   // two equal rows
    const int labelHeight = 22;
    const int gap         = 8;

    auto topRow    = area.removeFromTop (rowHeight);
    auto bottomRow = area;

    // --- Gain row
    {
        auto r = topRow;
        auto labelArea = r.removeFromTop (labelHeight);
        gainLabel.setBounds (labelArea);

        r.removeFromTop (gap);

        const int sliderSize = juce::jmin (r.getWidth(), r.getHeight());
        gainSlider.setBounds (r.withSizeKeepingCentre (sliderSize, sliderSize));
    }

    // --- Output row
    {
        auto r = bottomRow;
        auto labelArea = r.removeFromTop (labelHeight);
        outputLabel.setBounds (labelArea);

        r.removeFromTop (gap);

        const int sliderSize = juce::jmin (r.getWidth(), r.getHeight());
        outputSlider.setBounds (r.withSizeKeepingCentre (sliderSize, sliderSize));
    }
}


void MainView::gainSliderChanged()
{
    audioProcessor.getParameters().setGain (static_cast<float> (gainSlider.getValue()));
}

void MainView::outputSliderChanged()
{
    audioProcessor.getParameters().setOutputGain (static_cast<float> (outputSlider.getValue()));
}

bool MainView::keyPressed (const juce::KeyPress& key)
{
    if (key == juce::KeyPress::tabKey)
    {
        // Tab cycles focus between controls
        const auto prev = focusManager.getFocusedControl();
        ui_core::ControlId newFocusId;
        if (focusedControlId == kGainControlId)
            newFocusId = kOutputControlId;
        else
            newFocusId = kGainControlId;
        
        focusManager.setFocusedControl (newFocusId);
        audioProcessor.getParameters().setFocusedControlId (static_cast<int> (newFocusId));
        
        // Send hardware focus feedback
        if (hardwareOutput)
        {
            if (prev && *prev != newFocusId)
                hardwareOutput->setFocus (*prev, false);
            hardwareOutput->setFocus (newFocusId, true);
        }
        return true;
    }

    // Hardware control keys operate on focused control (default to gain if none focused)
    ui_core::ControlId targetId = focusedControlId != 0 ? focusedControlId : kGainControlId;
    
    auto ch = key.getTextCharacter();
    if (ch == 'h' || ch == 'H')
    {
        ui_core::HardwareControlEvent e { targetId, 0.375f, false };
        hardwareAdapter->processEvent (e);
        return true;
    }
    if (ch == 'j' || ch == 'J')
    {
        ui_core::HardwareControlEvent e { targetId, 0.025f, true };
        hardwareAdapter->processEvent (e);
        return true;
    }
    if (ch == 'k' || ch == 'K')
    {
        ui_core::HardwareControlEvent e { targetId, -0.025f, true };
        hardwareAdapter->processEvent (e);
        return true;
    }
    return false;
}
