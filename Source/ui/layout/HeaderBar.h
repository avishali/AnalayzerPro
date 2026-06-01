#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <mdsp_ui/UiContext.h>
#include "../../presets/PresetManager.h"
#include "../../presets/ABStateManager.h"
#include <functional>
#include <memory>

namespace AnalyzerPro { class ControlBinder; }

//==============================================================================
/**
    Header bar component with title and analyzer controls.
*/
class HeaderBar : public juce::Component
{
public:
    explicit HeaderBar (mdsp_ui::UiContext& ui);
    ~HeaderBar() override;

    void paint (juce::Graphics& g) override;
    void resized() override;

    void setControlBinder (AnalyzerPro::ControlBinder& binder);

    std::function<void (int)> onPeakRangeChanged;
    void setPeakRangeSelectedId (int id);
    void setRailOpen (bool isOpen);

    // FFT Zoom (dB range)
    juce::ComboBox dbRangeBox_;
    std::function<void()> onZoomReset;
    std::function<void()> onFreqPanLeft;
    std::function<void()> onFreqPanRight;
    std::function<void()> onFreqZoomIn;
    std::function<void()> onFreqZoomOut;
    std::function<void()> onFreqReset;
    std::function<void()> onResetPeaks;

    // Control Rail Toggle
    std::function<void()> onRailToggleClicked;

    // Module dropdown buttons — callback receives the button component as anchor for CallOutBox
    std::function<void (juce::Component*)> onSpectrumClicked;
    std::function<void (juce::Component*)> onScopesClicked;
    std::function<void (juce::Component*)> onMetersClicked;
    std::function<void (juce::Component*)> onTracesClicked;

    // Expose buttons so MainView can get their screen bounds for CallOutBox
    juce::TextButton& spectrumButton() noexcept { return spectrumBtn_; }
    juce::TextButton& scopesButton()   noexcept { return scopesBtn_;   }
    juce::TextButton& metersButton()   noexcept { return metersBtn_;   }
    juce::TextButton& tracesButton()   noexcept { return tracesBtn_;   }

    // State Management
    void setManagers (AnalyzerPro::presets::PresetManager* pm, AnalyzerPro::presets::ABStateManager* sm);
    
private:
    void updateActiveSlot();

    mdsp_ui::UiContext& ui_;
    AnalyzerPro::ControlBinder* controlBinder = nullptr;
    AnalyzerPro::presets::PresetManager* presetManager = nullptr;
    AnalyzerPro::presets::ABStateManager* abStateManager = nullptr;

    juce::Label titleLabel;
    juce::ComboBox peakRangeBox_;
    juce::TextButton zoomResetButton_;
    juce::TextButton freqPanLeftButton_;
    juce::TextButton freqPanRightButton_;
    juce::TextButton freqZoomInButton_;
    juce::TextButton freqZoomOutButton_;
    juce::TextButton freqResetButton_;
    juce::TextButton peakResetButton_;
    
    // Presets & State
    juce::TextButton presetButton; // Opens menu
    juce::TextButton saveButton;
    // juce::TextButton menuButton; // Removed or reused? Prefer A/B
    juce::TextButton slotAButton;
    juce::TextButton slotBButton;
    juce::ToggleButton bypassButton; // Bound to param
    juce::ToggleButton railToggleButton; // Toggle control rail visibility

    // Module dropdown buttons + scrollable container
    juce::TextButton   spectrumBtn_, scopesBtn_, metersBtn_, tracesBtn_;
    juce::Component    moduleBtnContainer_;
    juce::Viewport     moduleScrollPort_;

    std::unique_ptr<juce::LookAndFeel> headerBarLook_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HeaderBar)
};
