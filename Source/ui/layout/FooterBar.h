#pragma once

#include <functional>
#include <juce_gui_basics/juce_gui_basics.h>
#include <mdsp_ui/UiContext.h>
#include "DraggableParamValueLabel.h"
#include "../dev/DevLookPanelConfig.h"

namespace AnalyzerPro { class ControlBinder; }

//==============================================================================
/**
    Footer bar component.
    Always shows Peak Hold toggle and Release Time so they are accessible
    without opening a popup.
*/
class FooterBar : public juce::Component
{
public:
    explicit FooterBar (mdsp_ui::UiContext& ui);
    ~FooterBar() override;

    /** Call once after the main ControlBinder is ready to bind Hold + Release. */
    void setControlBinder (AnalyzerPro::ControlBinder& binder);

    void paint (juce::Graphics& g) override;
    void resized() override;

#if ANALYZERPRO_DEV_LOOK_PANEL
    std::function<void()> onDevLookClicked;
#endif

private:
    mdsp_ui::UiContext& ui_;

    juce::Label    statusLabel;

    // Always-visible analyzer controls
    juce::Label              holdLabel_;
    juce::ToggleButton       holdBtn_;
    juce::Label              releaseLabel_;
    AnalyzerPro::DraggableParamValueLabel releaseTimeValue_;
    juce::Label              holdDecayLabel_;
    AnalyzerPro::DraggableParamValueLabel holdDecayValue_;

#if ANALYZERPRO_DEV_LOOK_PANEL
    juce::TextButton devLookButton_ { "dev" };
#endif

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FooterBar)
};
