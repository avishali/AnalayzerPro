#include "HeaderBar.h"
#include "PixelSnap.h"
#include "../../control/ControlIds.h"
#include "../../control/ControlBinder.h"
#include <mdsp_ui/ButtonStyle.h>
#include <mdsp_ui/UiContext.h>
#include <cmath>

using namespace AnalyzerPro::Layout;

namespace
{
constexpr float kInsideStrokePx = 1.0f;

static void getStateColours (const juce::Button& b, const mdsp_ui::ButtonStyle& s,
                             juce::Colour& bg, juce::Colour& border)
{
    bg = s.bg;
    border = s.border;
    if (! b.isEnabled())
    {
        bg = s.bgDisabled;
        border = s.border.withMultipliedAlpha (0.5f);
    }
    else if (b.isDown())
    {
        bg = s.bgDown;
        border = s.borderDown;
    }
    else if (b.isOver())
    {
        bg = s.bgHover;
        border = s.borderHover;
    }
}

class HeaderBarLookAndFeel : public juce::LookAndFeel_V4
{
public:
    explicit HeaderBarLookAndFeel (mdsp_ui::UiContext& ui) : ui_ (ui) {}

    void drawButtonBackground (juce::Graphics& g, juce::Button& button,
                              const juce::Colour&, bool, bool) override
    {
        const juce::Rectangle<float> fillR = snapRectToPixels (button.getLocalBounds().toFloat());

        // Bypass button: drawn in drawToggleButton (JUCE ToggleButton uses drawToggleButton for rendering).
        if (button.getComponentID() == "bypass")
            return;

        const float radius = snapRadius (juce::jmin (ui_.metrics().rMed, fillR.getHeight() * 0.5f));
        const float strokeRadius = snapRadius (juce::jmax (0.0f, radius - 0.5f));
        const juce::Rectangle<float> strokeR = insetForInsideStroke (fillR, kInsideStrokePx);

        juce::Colour bgColour, borderColour;
        auto* toggle = dynamic_cast<juce::ToggleButton*> (&button);
        if (toggle != nullptr)
        {
            auto style = mdsp_ui::makeToggleButtonStyle (ui_, toggle->getToggleState(), toggle->isEnabled());
            getStateColours (button, style, bgColour, borderColour);
            g.setColour (bgColour);
            g.fillRoundedRectangle (fillR, radius);
            g.setColour (borderColour);
            g.drawRoundedRectangle (strokeR, strokeRadius, kInsideStrokePx);
            if (toggle->getToggleState())
            {
                const float indSize = 6.0f;
                const float indX = fillR.getX() + radius + 4.0f;
                const float indY = fillR.getCentreY();
                g.setColour (toggle->isEnabled() ? style.text : style.textDisabled);
                g.fillEllipse (indX - indSize * 0.5f, indY - indSize * 0.5f, indSize, indSize);
            }
        }
        else
        {
            auto style = mdsp_ui::makePrimaryButtonStyle (ui_, button.isEnabled());
            getStateColours (button, style, bgColour, borderColour);
            g.setColour (bgColour);
            g.fillRoundedRectangle (fillR, radius);
            g.setColour (borderColour);
            g.drawRoundedRectangle (strokeR, strokeRadius, kInsideStrokePx);
        }
        if (button.hasKeyboardFocus (true))
        {
            const float focusPx = ui_.metrics().strokeThin;
            auto focusBounds = snapRectToPixels (fillR.expanded (focusPx));
            g.setColour (toggle != nullptr
                ? mdsp_ui::makeToggleButtonStyle (ui_, static_cast<juce::ToggleButton*>(&button)->getToggleState(), true).focusRing
                : mdsp_ui::makePrimaryButtonStyle (ui_, true).focusRing);
            g.drawRoundedRectangle (focusBounds.toFloat(), radius + focusPx, focusPx);
        }
    }

    void drawToggleButton (juce::Graphics& g, juce::ToggleButton& button,
                          bool, bool) override
    {
        const juce::Rectangle<float> fillR = snapRectToPixels (button.getLocalBounds().toFloat());

        // Bypass button: icon-only pill. For ToggleButton, JUCE draws via drawToggleButton (not drawButtonBackground).
        if (button.getComponentID() == "bypass")
        {
            const float radius = snapRadius (juce::jmin (ui_.metrics().rMed, fillR.getHeight() * 0.5f));
            const float strokeRadius = snapRadius (juce::jmax (0.0f, radius - 0.5f));
            const juce::Rectangle<float> strokeR = insetForInsideStroke (fillR, kInsideStrokePx);
            auto style = mdsp_ui::makeToggleButtonStyle (ui_, button.getToggleState(), button.isEnabled());
            juce::Colour bgColour, borderColour;
            getStateColours (button, style, bgColour, borderColour);
            g.setColour (bgColour);
            g.fillRoundedRectangle (fillR, radius);
            g.setColour (borderColour);
            g.drawRoundedRectangle (strokeR, strokeRadius, kInsideStrokePx);
            const float indSize = 6.0f;
            const float indX = fillR.getX() + radius + 4.0f;
            const float indY = fillR.getCentreY();
            if (button.getToggleState())
            {
                g.setColour (button.isEnabled() ? style.text : style.textDisabled);
                g.fillEllipse (indX - indSize * 0.5f, indY - indSize * 0.5f, indSize, indSize);
            }
            juce::Rectangle<float> textBounds = fillR;
            const float leftMargin = button.getToggleState()
                ? (indX + indSize * 0.5f + 4.0f - fillR.getX())
                : (radius + 4.0f);
            textBounds.removeFromLeft (leftMargin);
            g.setColour (button.isEnabled() ? style.text : style.textDisabled);
            g.setFont (ui_.type().labelFont());
            g.drawFittedText (button.getButtonText().isEmpty() ? "BYPASS" : button.getButtonText(),
                             textBounds.toNearestInt(), juce::Justification::centredLeft, 1);
            if (button.hasKeyboardFocus (true))
            {
                const float focusPx = ui_.metrics().strokeThin;
                auto focusBounds = snapRectToPixels (fillR.expanded (focusPx));
                g.setColour (style.focusRing);
                g.drawRoundedRectangle (focusBounds.toFloat(), radius + focusPx, focusPx);
            }
            return;
        }

        // Other toggle buttons (rail, etc.): use pill style from drawButtonBackground
        const float radius = snapRadius (juce::jmin (ui_.metrics().rMed, fillR.getHeight() * 0.5f));
        const float strokeRadius = snapRadius (juce::jmax (0.0f, radius - 0.5f));
        const juce::Rectangle<float> strokeR = insetForInsideStroke (fillR, kInsideStrokePx);
        auto style = mdsp_ui::makeToggleButtonStyle (ui_, button.getToggleState(), button.isEnabled());
        juce::Colour bgColour, borderColour;
        getStateColours (button, style, bgColour, borderColour);
        g.setColour (bgColour);
        g.fillRoundedRectangle (fillR, radius);
        g.setColour (borderColour);
        g.drawRoundedRectangle (strokeR, strokeRadius, kInsideStrokePx);
        if (button.getToggleState())
        {
            const float indSize = 6.0f;
            const float indX = fillR.getX() + radius + 4.0f;
            const float indY = fillR.getCentreY();
            g.setColour (button.isEnabled() ? style.text : style.textDisabled);
            g.fillEllipse (indX - indSize * 0.5f, indY - indSize * 0.5f, indSize, indSize);
        }
        if (button.hasKeyboardFocus (true))
        {
            const float focusPx = ui_.metrics().strokeThin;
            auto focusBounds = snapRectToPixels (fillR.expanded (focusPx));
            g.setColour (style.focusRing);
            g.drawRoundedRectangle (focusBounds.toFloat(), radius + focusPx, focusPx);
        }
        juce::Rectangle<float> textBounds = fillR;
        const float indSize = 6.0f;
        const float leftMargin = button.getToggleState()
            ? (radius + 4.0f + indSize * 0.5f + 4.0f)
            : (radius + 4.0f);
        textBounds.removeFromLeft (leftMargin);
        g.setColour (button.isEnabled() ? style.text : style.textDisabled);
        g.setFont (ui_.type().labelFont());
        g.drawFittedText (button.getButtonText(), textBounds.toNearestInt(), juce::Justification::centredLeft, 1);
    }

    void drawButtonText (juce::Graphics& g, juce::TextButton& button,
                         bool shouldDrawHighlighted, bool shouldDrawDown) override
    {
        // Bypass button: icon-only pill. Call base to invoke drawToggleButton (JUCE flow).
        if (button.getComponentID() == "bypass")
        {
            juce::LookAndFeel_V4::drawButtonText (g, button, shouldDrawHighlighted, shouldDrawDown);
            return;
        }

        const juce::Rectangle<float> fillR = snapRectToPixels (button.getLocalBounds().toFloat());
        auto* toggle = dynamic_cast<juce::ToggleButton*> (&button);
        if (toggle != nullptr)
        {
            auto style = mdsp_ui::makeToggleButtonStyle (ui_, toggle->getToggleState(), toggle->isEnabled());
            juce::Rectangle<float> textBounds = fillR;
            const float radius = ui_.metrics().rMed;
            const float indSize = 6.0f;
            const float leftMargin = toggle->getToggleState()
                ? (radius + 4.0f + indSize * 0.5f + 4.0f)
                : (radius + 4.0f);
            textBounds.removeFromLeft (leftMargin);
            g.setColour (toggle->isEnabled() ? style.text : style.textDisabled);
            g.setFont (ui_.type().labelFont());
            g.drawFittedText (button.getButtonText(), textBounds.toNearestInt(), juce::Justification::centredLeft, 1);
        }
        else
        {
            auto style = mdsp_ui::makePrimaryButtonStyle (ui_, button.isEnabled());
            g.setColour (button.isEnabled() ? style.text : style.textDisabled);
            g.setFont (ui_.type().labelFont());
            g.drawFittedText (button.getButtonText(), fillR.toNearestInt(), juce::Justification::centred, 1);
        }
    }

private:
    mdsp_ui::UiContext& ui_;
};
} // namespace

//==============================================================================
HeaderBar::HeaderBar (mdsp_ui::UiContext& ui)
    : ui_ (ui)
{
    headerBarLook_ = std::make_unique<HeaderBarLookAndFeel> (ui_);
    auto* laf = headerBarLook_.get();
    presetButton.setLookAndFeel (laf);
    saveButton.setLookAndFeel (laf);
    slotAButton.setLookAndFeel (laf);
    slotBButton.setLookAndFeel (laf);
    bypassButton.setLookAndFeel (laf);
    railToggleButton.setLookAndFeel (laf);

    const auto& theme = ui_.theme();
    const auto& type = ui_.type();

    titleLabel.setText ("AnalyzerPro", juce::dontSendNotification);
    titleLabel.setFont (type.titleFont());
    titleLabel.setJustificationType (juce::Justification::centred);
    titleLabel.setColour (juce::Label::textColourId, theme.lightGrey);
    addAndMakeVisible (titleLabel);

    peakRangeBox_.setTooltip ("Peak display range (dB). Drag analyzer vertical axis to change.");

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

    // Control Rail Toggle
    railToggleButton.setButtonText (juce::String (juce::CharPointer_UTF8 ("\xe2\x98\xb0")));  // ☰ hamburger
    railToggleButton.setClickingTogglesState (true);
    railToggleButton.setToggleState (true, juce::dontSendNotification); // Default: rail is open
    railToggleButton.setColour (juce::ToggleButton::tickColourId, theme.accent);
    railToggleButton.setColour (juce::TextButton::buttonColourId, theme.panel);
    railToggleButton.setTooltip ("Toggle control rail visibility");
    railToggleButton.onClick = [this]
    {
        if (onRailToggleClicked)
            onRailToggleClicked();
    };
    addAndMakeVisible (railToggleButton);
}

HeaderBar::~HeaderBar()
{
    presetButton.setLookAndFeel (nullptr);
    saveButton.setLookAndFeel (nullptr);
    slotAButton.setLookAndFeel (nullptr);
    slotBButton.setLookAndFeel (nullptr);
    bypassButton.setLookAndFeel (nullptr);
    railToggleButton.setLookAndFeel (nullptr);
}

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
    const int presetW = juce::jlimit (1, 999, m.headerButtonW);
    const int bypassW = juce::jlimit (1, 999, m.headerButtonW);

    const int rowHeightTarget = juce::jmin (m.headerButtonH, getHeight() - 2 * paddingY);
    const int buttonH = juce::jlimit (1, getHeight() - 2 * paddingY, static_cast<int> (std::round (static_cast<float> (rowHeightTarget))));

    auto barArea = getLocalBounds();
    const int rightMargin = 48;  // extra margin so bypass/rail aren't clipped by host/window
    const juce::Rectangle<int> row = snapRectToPixels (barArea.reduced (paddingX, paddingY).withTrimmedRight (rightMargin).toNearestInt());
    const int rowCentreY = row.getCentreY();
    const int y = static_cast<int> (std::round (static_cast<float> (rowCentreY) - buttonH * 0.5f));

    const int rightZoneWidth = comboW + gapX + presetW + gapX + smallBtnW + gapX + smallBtnW + gapX + bypassW + gapX + smallBtnW;
    const juce::Rectangle<int> rightZone (row.getRight() - rightZoneWidth, row.getY(), rightZoneWidth, row.getHeight());
    int x = rightZone.getX();

    peakRangeBox_.setBounds (x, y, comboW, buttonH);
    x += comboW + gapX;

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

    const int railW = juce::jmin (smallBtnW, rightZone.getRight() - x);
    railToggleButton.setBounds (x, y, railW, buttonH);

    const juce::Rectangle<int> area (row.getX(), row.getY(), rightZone.getX() - row.getX(), row.getHeight());
    const int titleCentreY = area.getCentreY();
    const int titleTop = static_cast<int> (std::round (static_cast<float> (titleCentreY) - ui_.type().titleH * 0.5f));
    const int titleH = static_cast<int> (std::round (ui_.type().titleH + 6.0f));
    const int titleWidth = juce::jmax (80, area.getWidth());
    titleLabel.setBounds (area.getX(), titleTop, titleWidth, titleH);
    titleLabel.setJustificationType (juce::Justification::centred);
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
