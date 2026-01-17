#include "HeaderBar.h"
#include "HeaderBar.h"
#include "../../control/ControlIds.h"
#include "../../control/ControlBinder.h"

//==============================================================================
HeaderBar::HeaderBar (mdsp_ui::UiContext& ui)
    : ui_ (ui)
{
    const auto& theme = ui_.theme();
    const auto& type = ui_.type();

    titleLabel.setText ("AnalyzerPro", juce::dontSendNotification);
    titleLabel.setFont (type.titleFont());
    titleLabel.setJustificationType (juce::Justification::centred);
    titleLabel.setColour (juce::Label::textColourId, theme.lightGrey);
    addAndMakeVisible (titleLabel);

    // Mode Toggles (replacing combo)
    auto initModeBtn = [&](juce::TextButton& b, const juce::String& text, int id)
    {
        b.setButtonText (text);
        b.setRadioGroupId (1001);
        b.setClickingTogglesState (true);
        b.setColour (juce::TextButton::buttonColourId, theme.background.brighter (0.05f));
        b.setColour (juce::TextButton::buttonOnColourId, theme.accent);
        b.onClick = [this, id] { if (onModeChanged) onModeChanged (id); };
        addAndMakeVisible (b);
    };
    initModeBtn (fftButton_, "FFT", 1);
    initModeBtn (bandButton_, "BAND", 2);
    initModeBtn (logButton_, "LOG", 3);
    fftButton_.setToggleState (true, juce::dontSendNotification);

    // FFT Size control
    fftSizeCombo_.addItem ("1024", 1);
    fftSizeCombo_.addItem ("2048", 2);
    fftSizeCombo_.addItem ("4096", 3);
    fftSizeCombo_.addItem ("8192", 4);
    fftSizeCombo_.setSelectedId (2, juce::dontSendNotification);
    fftSizeCombo_.setTooltip ("FFT Size");
    addAndMakeVisible (fftSizeCombo_);

    // Averaging control
    averagingCombo_.addItem ("Off", 1);
    averagingCombo_.addItem ("50 ms", 2);
    averagingCombo_.addItem ("100 ms", 3);
    averagingCombo_.addItem ("250 ms", 4);
    averagingCombo_.addItem ("500 ms", 5);
    averagingCombo_.addItem ("1 s", 6);
    averagingCombo_.setSelectedId (3, juce::dontSendNotification);
    averagingCombo_.setTooltip ("Averaging");
    addAndMakeVisible (averagingCombo_);

    resetPeaksButton_.onClick = [this]
    {
        if (onResetPeaks)
            onResetPeaks();
    };
    addAndMakeVisible (resetPeaksButton_);
    
    // Preset & State Buttons
    presetButton.setButtonText ("Preset");
    presetButton.setTooltip ("Load Preset");
    presetButton.setColour (juce::TextButton::buttonColourId, theme.panel);
    presetButton.onClick = [this]
    {
        if (presetManager)
        {
            juce::PopupMenu m;
            m.addItem ("Factory", [this] { presetManager->loadFactory(); });
            m.addItem ("Default", [this] { presetManager->loadDefaultOrFactory(); });
            m.addItem ("Save as Default", [this] { presetManager->saveDefault(); });
            m.addSeparator();
            
            auto presets = presetManager->listPresets();
            for (const auto& p : presets)
                m.addItem (p, [this, p] { presetManager->loadPreset (p); });

            m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&presetButton));
        }
    };
    addAndMakeVisible (presetButton);

    saveButton.setButtonText ("Save");
    saveButton.setTooltip ("Save Preset");
    saveButton.setColour (juce::TextButton::buttonColourId, theme.panel);
    saveButton.onClick = [this]
    {
        if (presetManager)
        {
            auto w = std::make_shared<juce::AlertWindow> ("Save Preset", "Enter name:", juce::MessageBoxIconType::NoIcon);
            w->addTextEditor ("name", presetManager->getCurrentPresetName());
            w->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
            w->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
            w->enterModalState (true, juce::ModalCallbackFunction::create ([this, w] (int result)
            {
                if (result == 1)
                {
                    auto name = w->getTextEditorContents ("name");
                    if (name.isNotEmpty())
                        presetManager->savePreset (name);
                }
            }));
        }
    };
    addAndMakeVisible (saveButton);

    // A/B Slots
    auto initSlotBtn = [&](juce::TextButton& b, const juce::String& text, AnalyzerPro::presets::ABStateManager::Slot slot)
    {
        b.setButtonText (text);
        b.setRadioGroupId (2002);
        b.setClickingTogglesState (true);
        b.setColour (juce::TextButton::buttonColourId, theme.panel);
        b.setColour (juce::TextButton::buttonOnColourId, theme.accent);
        b.onClick = [this, slot]
        {
            if (abStateManager)
                abStateManager->setActiveSlot (slot);
        };
        addAndMakeVisible (b);
    };
    initSlotBtn (slotAButton, "A", AnalyzerPro::presets::ABStateManager::Slot::A);
    initSlotBtn (slotBButton, "B", AnalyzerPro::presets::ABStateManager::Slot::B);
    slotAButton.setToggleState (true, juce::dontSendNotification);

    // Bypass
    bypassButton.setButtonText ("BYPASS");
    bypassButton.setClickingTogglesState (true);
    bypassButton.setColour (juce::ToggleButton::tickColourId, theme.accent); 
    bypassButton.setColour (juce::TextButton::buttonColourId, theme.panel); // If using TextButton vs ToggleButton logic
    // Wait, declaration is ToggleButton.
    // Let's style it like others?
    // ToggleButton style is different. Let's assume standard toggle behavior for now.
    addAndMakeVisible (bypassButton);
}

HeaderBar::~HeaderBar() = default;

void HeaderBar::paint (juce::Graphics& g)
{
    const auto& theme = ui_.theme();

    // Dark background with subtle contrast
    g.fillAll (theme.black);
    g.setColour (theme.borderDivider);
    g.fillRect (getLocalBounds().removeFromBottom (1));
}

void HeaderBar::resized()
{
    // Layout constants (normalized)
    static constexpr int headerPadX = 12;
    static constexpr int headerGap = 8;
    static constexpr int controlH = 22;
    static constexpr int comboW = 112;
    static constexpr int smallBtnW = 22;
    static constexpr int modeBtnW = 60; // Button width for toggles
    
    const auto& m = ui_.metrics();
    auto area = getLocalBounds().reduced (headerPadX, 0);
    const int centerY = area.getCentreY();
    const int controlTop = centerY - controlH / 2;
    const int buttonTop = centerY - controlH / 2;

    // Right zone: Peak Range + Reset + Preset/Save/Menu buttons
    // Removed DbRange
    // Right zone: Peak Range + Reset + Preset/Save + A/B + Bypass
    const int rightZoneWidth = comboW        // Peak Range
                              + headerGap
                              + smallBtnW    // Reset
                              + headerGap
                              + m.headerButtonW * 2  // Preset/Save
                              + headerGap
                              + smallBtnW * 2 // A/B
                              + headerGap
                              + m.headerButtonW; // Bypass

    auto rightZone = area.removeFromRight (rightZoneWidth);
    
    // Bypass
    auto buttonArea = rightZone.removeFromRight (m.headerButtonW);
    bypassButton.setBounds (buttonArea.getX(), controlTop, m.headerButtonW, controlH);
    rightZone.removeFromRight (headerGap);

    // A/B
    slotBButton.setBounds (rightZone.removeFromRight (smallBtnW).getX(), controlTop, smallBtnW, controlH);
    slotAButton.setBounds (rightZone.removeFromRight (smallBtnW).getX(), controlTop, smallBtnW, controlH);
    rightZone.removeFromRight (headerGap);
    
    // Save/Preset
    buttonArea = rightZone.removeFromRight (m.headerButtonW);
    saveButton.setBounds (buttonArea.getX(), buttonTop, m.headerButtonW, controlH);
    rightZone.removeFromRight (headerGap);
    
    buttonArea = rightZone.removeFromRight (m.headerButtonW);
    presetButton.setBounds (buttonArea.getX(), buttonTop, m.headerButtonW, controlH);
    rightZone.removeFromRight (headerGap);
    
    // Reset / PeakRange
    resetPeaksButton_.setBounds (rightZone.removeFromRight (smallBtnW).getX(), controlTop, smallBtnW, smallBtnW);
    rightZone.removeFromRight (headerGap);
    
    peakRangeBox_.setBounds (rightZone.removeFromRight (comboW).getX(), controlTop, comboW, controlH);

    // Left zone: Mode Buttons (x3) + FFT Size + Averaging
    const int leftZoneWidth = (modeBtnW * 3 + headerGap) // Mode Group
                              + headerGap
                              + comboW * 2               // FFT + Avg
                              + headerGap * 2;
    auto leftZone = area.removeFromLeft (leftZoneWidth);
    
    // Mode Buttons
    fftButton_.setBounds (leftZone.removeFromLeft (modeBtnW).getX(), controlTop, modeBtnW, controlH);
    bandButton_.setBounds (leftZone.removeFromLeft (modeBtnW).getX(), controlTop, modeBtnW, controlH);
    logButton_.setBounds (leftZone.removeFromLeft (modeBtnW).getX(), controlTop, modeBtnW, controlH);
    leftZone.removeFromLeft (headerGap);
    
    fftSizeCombo_.setBounds (leftZone.removeFromLeft (comboW).getX(), controlTop, comboW, controlH);
    leftZone.removeFromLeft (headerGap);
    
    averagingCombo_.setBounds (leftZone.removeFromLeft (comboW).getX(), controlTop, comboW, controlH);

    // Center zone: Title (centered, fills remaining space)
    const int titleTop = centerY - static_cast<int> (ui_.type().titleH / 2.0f);
    const int titleWidth = juce::jmax (80, area.getWidth());
    titleLabel.setBounds (area.getX(), titleTop, titleWidth, static_cast<int> (ui_.type().titleH + 6.0f));
    titleLabel.setJustificationType (juce::Justification::centred);
}

void HeaderBar::setControlBinder (AnalyzerPro::ControlBinder& binder)
{
    controlBinder = &binder;
    
    if (controlBinder != nullptr)
    {
        // Mode binding is now Manual (via onModeChanged callback)
        // controlBinder->bindCombo (AnalyzerPro::ControlId::AnalyzerMode, modeCombo_);
        
        controlBinder->bindCombo (AnalyzerPro::ControlId::AnalyzerFftSize, fftSizeCombo_);
        controlBinder->bindCombo (AnalyzerPro::ControlId::AnalyzerAveraging, averagingCombo_);
        
        controlBinder->bindToggle (AnalyzerPro::ControlId::MasterBypass, bypassButton);
    }
}

void HeaderBar::setManagers (AnalyzerPro::presets::PresetManager* pm, AnalyzerPro::presets::ABStateManager* sm)
{
    presetManager = pm;
    abStateManager = sm;
    
    if (abStateManager)
    {
        abStateManager->onSlotChanged = [this] (AnalyzerPro::presets::ABStateManager::Slot slot)
        {
            // Update UI on message thread (callback likely on msg thread but safe to force)
            juce::MessageManager::callAsync ([this, slot]
            {
                if (slot == AnalyzerPro::presets::ABStateManager::Slot::A)
                    slotAButton.setToggleState (true, juce::dontSendNotification);
                else
                    slotBButton.setToggleState (true, juce::dontSendNotification);
            });
        };
        
        // Init state
        updateActiveSlot();
    }
    
    presetButton.setEnabled (presetManager != nullptr);
    saveButton.setEnabled (presetManager != nullptr);
    slotAButton.setEnabled (abStateManager != nullptr);
    slotBButton.setEnabled (abStateManager != nullptr);
}

void HeaderBar::updateActiveSlot()
{
    if (abStateManager)
    {
        auto slot = abStateManager->getActiveSlot();
        if (slot == AnalyzerPro::presets::ABStateManager::Slot::A)
            slotAButton.setToggleState (true, juce::dontSendNotification);
        else
            slotBButton.setToggleState (true, juce::dontSendNotification);
    }
}

void HeaderBar::setPeakRangeSelectedId (int id)
{
    peakRangeBox_.setSelectedId (id, juce::dontSendNotification);
}

void HeaderBar::setMode (int modeIndex)
{
    if (modeIndex == 1) fftButton_.setToggleState (true, juce::dontSendNotification);
    else if (modeIndex == 2) bandButton_.setToggleState (true, juce::dontSendNotification);
    else if (modeIndex == 3) logButton_.setToggleState (true, juce::dontSendNotification);
}
