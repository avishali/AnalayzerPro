#include "HeaderBar.h"
#include "PixelSnap.h"
#include "../../control/ControlIds.h"
#include "../../control/ControlBinder.h"
#include <mdsp_ui/UiContext.h>
#include <cmath>

using namespace AnalyzerPro::Layout;

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

    peakRangeBox_.setTooltip ("Peak display range (dB). Drag analyzer vertical axis to change.");

    // FFT Zoom (dB range) combo
    dbRangeBox_.addItem ("-60 dB",  1);
    dbRangeBox_.addItem ("-90 dB",  2);
    dbRangeBox_.addItem ("-120 dB", 3);
    dbRangeBox_.setSelectedId (2, juce::dontSendNotification);
    dbRangeBox_.setTooltip ("FFT vertical zoom presets; drag or wheel the grid for continuous zoom.");
    addAndMakeVisible (dbRangeBox_);

    // Zoom reset button — returns range to -90 dB default
    zoomResetButton_.setButtonText (juce::CharPointer_UTF8 ("\xe2\x86\xba")); // ↺
    zoomResetButton_.setTooltip ("Reset view to 10 Hz-Nyquist and -90 dB");
    zoomResetButton_.setColour (juce::TextButton::buttonColourId, theme.panel);
    zoomResetButton_.setColour (juce::TextButton::textColourOffId, theme.text);
    zoomResetButton_.setColour (juce::TextButton::textColourOnId, theme.text);
    zoomResetButton_.onClick = [this] { if (onZoomReset) onZoomReset(); };
    addAndMakeVisible (zoomResetButton_);

    auto initTinyButton = [&] (juce::TextButton& b, const juce::String& text, const juce::String& tip, std::function<void()>* cb)
    {
        b.setButtonText (text);
        b.setTooltip (tip);
        b.setColour (juce::TextButton::buttonColourId, theme.panel);
        b.setColour (juce::TextButton::textColourOffId, theme.text);
        b.setColour (juce::TextButton::textColourOnId, theme.text);
        b.onClick = [cb] { if (cb != nullptr && *cb) (*cb)(); };
        addAndMakeVisible (b);
    };
    initTinyButton (freqPanLeftButton_,  "<", "Pan to lower frequencies", &onFreqPanLeft);
    initTinyButton (freqPanRightButton_, ">", "Pan to higher frequencies", &onFreqPanRight);
    initTinyButton (freqZoomOutButton_,  "-", "Zoom out frequency range", &onFreqZoomOut);
    initTinyButton (freqZoomInButton_,   "+", "Zoom in frequency range", &onFreqZoomIn);
    initTinyButton (freqResetButton_,    "F", "Reset frequency range to 10 Hz-Nyquist", &onFreqReset);
    initTinyButton (peakResetButton_,    "reset", "Reset analyzer peaks and holds", &onResetPeaks);

    // Preset & State Buttons
    presetButton.setButtonText ("Preset");
    presetButton.setTooltip ("Load Preset");
    presetButton.setColour (juce::TextButton::buttonColourId, theme.panel);
    presetButton.setColour (juce::TextButton::textColourOffId, theme.text);
    presetButton.setColour (juce::TextButton::textColourOnId, theme.text);
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
    saveButton.setColour (juce::TextButton::textColourOffId, theme.text);
    saveButton.setColour (juce::TextButton::textColourOnId, theme.text);
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
        b.setColour (juce::TextButton::buttonOnColourId, theme.panel.brighter (0.22f));
        b.setColour (juce::TextButton::textColourOffId, theme.text);
        b.setColour (juce::TextButton::textColourOnId, theme.text);
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
    slotAButton.setTooltip ("Compare state A");
    slotBButton.setTooltip ("Compare state B");

    // Bypass (uses MDSP Toggle Pill icon from ui_assets)
    bypassButton.setComponentID ("bypass");
    bypassButton.setButtonText ({});
    bypassButton.setClickingTogglesState (true);
    bypassButton.setColour (juce::ToggleButton::tickColourId, theme.accent); 
    bypassButton.setColour (juce::TextButton::buttonColourId, theme.panel); // If using TextButton vs ToggleButton logic
    bypassButton.setTooltip ("Bypass analyzer processing");
    addAndMakeVisible (bypassButton);

    // Module settings tabs (Spectrum / Scopes / Meters / Traces) inside scrollable container
    auto initModuleBtn = [&] (juce::TextButton& b, const juce::String& text,
                              juce::Colour textCol,
                              std::function<void()>& cb)
    {
        b.setComponentID ("module-" + text.toLowerCase());
        b.setButtonText (text);
        b.setClickingTogglesState (false);
        b.setColour (juce::TextButton::buttonColourId,  theme.background.brighter (0.08f));
        b.setColour (juce::TextButton::buttonOnColourId, theme.panel.brighter (0.24f));
        b.setColour (juce::TextButton::textColourOffId, textCol);
        b.setColour (juce::TextButton::textColourOnId, textCol.brighter (0.22f));
        b.onClick = [&cb] { if (cb) cb(); };
        moduleBtnContainer_.addAndMakeVisible (b);
    };
    initModuleBtn (spectrumBtn_, "Spectrum", theme.seriesPeak,   onSpectrumClicked);
    initModuleBtn (scopesBtn_,   "Scopes",   theme.seriesStereo, onScopesClicked);
    initModuleBtn (metersBtn_,   "Meters",   theme.seriesLeft,   onMetersClicked);
    initModuleBtn (tracesBtn_,   "Traces",   theme.seriesMono,   onTracesClicked);
    spectrumBtn_.setTooltip ("Spectrum analyser settings");
    scopesBtn_  .setTooltip ("Stereo scope settings");
    metersBtn_  .setTooltip ("Level meter settings");
    tracesBtn_  .setTooltip ("Trace visibility settings");

    // Scroll port — horizontal scroll via trackpad swipe, no visible scrollbars
    moduleScrollPort_.setViewedComponent (&moduleBtnContainer_, false);
    moduleScrollPort_.setScrollBarsShown (false, false);
    moduleScrollPort_.setScrollOnDragMode (juce::Viewport::ScrollOnDragMode::never);
    addAndMakeVisible (moduleScrollPort_);

    // Control Rail Toggle
    railToggleButton.setComponentID ("rail-toggle");
    railToggleButton.setClickingTogglesState (true);
    railToggleButton.setColour (juce::ToggleButton::tickColourId, theme.accent);
    railToggleButton.setColour (juce::TextButton::buttonColourId, theme.panel);
    setRailOpen (true);
    railToggleButton.onClick = [this]
    {
        if (onRailToggleClicked)
            onRailToggleClicked();
    };
    addAndMakeVisible (railToggleButton);
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
    const auto& m = ui_.metrics();
    const int paddingX = 12;
    const int paddingY = 4;
    const int gapX = 8;
    const int comboW = 112;
    const int smallBtnW = 22;
    const int peakResetW = 44;
    const int railBtnW = juce::jmax (60, juce::jlimit (1, 999, m.headerButtonW));
    const int presetW = juce::jlimit (1, 999, m.headerButtonW);
    const int bypassW = juce::jlimit (1, 999, m.headerButtonW);

    const int rowHeightTarget = juce::jmin (m.headerButtonH, getHeight() - 2 * paddingY);
    const int buttonH = juce::jlimit (1, getHeight() - 2 * paddingY, static_cast<int> (std::round (static_cast<float> (rowHeightTarget))));

    auto barArea = getLocalBounds();
    const int rightMargin = 48;  // extra margin so bypass/rail aren't clipped by host/window
    const juce::Rectangle<int> row = snapRectToPixels (barArea.reduced (paddingX, paddingY).withTrimmedRight (rightMargin).toNearestInt());
    const int rowCentreY = row.getCentreY();
    const int y = static_cast<int> (std::round (static_cast<float> (rowCentreY) - buttonH * 0.5f));

    const bool hasPeakRange = peakRangeBox_.getParentComponent() == this;
    const int peakRangeW = hasPeakRange ? (comboW + gapX) : 0;
    const int zoomResetW = smallBtnW;
    const int navZoneWidth = smallBtnW * 5 + peakResetW + gapX * 6;
    const int rightZoneWidth = navZoneWidth + peakRangeW + comboW + gapX + zoomResetW + gapX + presetW + gapX + presetW + gapX + smallBtnW + gapX + smallBtnW + gapX + bypassW + gapX + railBtnW;
    const juce::Rectangle<int> rightZone (row.getRight() - rightZoneWidth, row.getY(), rightZoneWidth, row.getHeight());
    int x = rightZone.getX();

    freqPanLeftButton_.setBounds (x, y, smallBtnW, buttonH);
    x += smallBtnW + gapX;
    freqPanRightButton_.setBounds (x, y, smallBtnW, buttonH);
    x += smallBtnW + gapX;
    freqZoomOutButton_.setBounds (x, y, smallBtnW, buttonH);
    x += smallBtnW + gapX;
    freqZoomInButton_.setBounds (x, y, smallBtnW, buttonH);
    x += smallBtnW + gapX;
    freqResetButton_.setBounds (x, y, smallBtnW, buttonH);
    x += smallBtnW + gapX;
    peakResetButton_.setBounds (x, y, peakResetW, buttonH);
    x += peakResetW + gapX;

    if (hasPeakRange)
    {
        peakRangeBox_.setBounds (x, y, comboW, buttonH);
        x += comboW + gapX;
    }

    dbRangeBox_.setBounds (x, y, comboW, buttonH);
    x += comboW + gapX;

    zoomResetButton_.setBounds (x, y, zoomResetW, buttonH);
    x += zoomResetW + gapX;

    presetButton.setBounds (x, y, presetW, buttonH);
    x += presetW + gapX;

    saveButton.setBounds (x, y, presetW, buttonH);
    x += presetW + gapX;

    slotAButton.setBounds (x, y, smallBtnW, buttonH);
    x += smallBtnW + gapX;

    slotBButton.setBounds (x, y, smallBtnW, buttonH);
    x += smallBtnW + gapX;

    bypassButton.setBounds (x, y, bypassW, buttonH);
    x += bypassW + gapX;

    const int railW = juce::jmin (railBtnW, rightZone.getRight() - x);
    railToggleButton.setBounds (x, y, railW, buttonH);

    // Title area: fixed title on the left, scrollable module buttons to the right
    const juce::Rectangle<int> leftArea (row.getX(), row.getY(), rightZone.getX() - row.getX(), row.getHeight());
    const int titleCentreY = leftArea.getCentreY();
    const int titleTop = static_cast<int> (std::round (static_cast<float> (titleCentreY) - ui_.type().titleH * 0.5f));
    const int titleH   = static_cast<int> (std::round (ui_.type().titleH + 6.0f));
    constexpr int kTitleFixedW = 110;
    constexpr int kModuleBtnW  = 72;
    constexpr int kModuleGap   = 6;
    constexpr int kNumBtns     = 4;
    constexpr int kContainerW  = kNumBtns * kModuleBtnW + (kNumBtns - 1) * kModuleGap;

    titleLabel.setBounds (leftArea.getX(), titleTop, kTitleFixedW, titleH);
    titleLabel.setJustificationType (juce::Justification::centredLeft);

    // Module buttons live inside a scrollable viewport so they're always reachable
    const int portX = leftArea.getX() + kTitleFixedW + kModuleGap;
    const int portW = juce::jmax (0, leftArea.getRight() - portX);
    moduleScrollPort_.setBounds (portX, y, portW, buttonH);

    // Container is exactly wide enough for all 4 buttons side-by-side
    moduleBtnContainer_.setBounds (0, 0, kContainerW, buttonH);
    int mx = 0;
    spectrumBtn_.setBounds (mx, 0, kModuleBtnW, buttonH); mx += kModuleBtnW + kModuleGap;
    scopesBtn_  .setBounds (mx, 0, kModuleBtnW, buttonH); mx += kModuleBtnW + kModuleGap;
    metersBtn_  .setBounds (mx, 0, kModuleBtnW, buttonH); mx += kModuleBtnW + kModuleGap;
    tracesBtn_  .setBounds (mx, 0, kModuleBtnW, buttonH);
}

void HeaderBar::setControlBinder (AnalyzerPro::ControlBinder& binder)
{
    controlBinder = &binder;
    
    if (controlBinder != nullptr)
        controlBinder->bindToggle (AnalyzerPro::ControlId::MasterBypass, bypassButton);
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

void HeaderBar::setRailOpen (bool isOpen)
{
    railOpen_ = isOpen;
    railToggleButton.setToggleState (isOpen, juce::dontSendNotification);
    railToggleButton.setButtonText (isOpen ? "Hide" : "Show");
    railToggleButton.setTooltip (isOpen ? "Collapse right control rail" : "Expand right control rail");
    updateModuleButtons();
}

void HeaderBar::setActiveModule (ActiveModule module)
{
    if (activeModule_ == module)
        return;

    activeModule_ = module;
    updateModuleButtons();
}

void HeaderBar::updateModuleButtons()
{
    const bool showActive = railOpen_;
    spectrumBtn_.setToggleState (showActive && activeModule_ == ActiveModule::Spectrum, juce::dontSendNotification);
    scopesBtn_  .setToggleState (showActive && activeModule_ == ActiveModule::Scopes,   juce::dontSendNotification);
    metersBtn_  .setToggleState (showActive && activeModule_ == ActiveModule::Meters,   juce::dontSendNotification);
    tracesBtn_  .setToggleState (showActive && activeModule_ == ActiveModule::Traces,   juce::dontSendNotification);
}
