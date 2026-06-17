#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <mdsp_ui/UiContext.h>
#include "../../control/ControlBinder.h"
#include <mdsp_ui/controls/SectionHeader.h>
#include <mdsp_ui/controls/ChoiceRow.h>
#include <mdsp_ui/controls/ToggleRow.h>
#include "DraggableParamValueLabel.h"
#include "../theme/TraceColors.h"
#include <functional>
#include <array>

//==============================================================================
/**
    Control rail component for right-side controls.
    Contains placeholder sections for control groups.
*/
class ControlRail : public juce::Component
{
public:
    enum class ActiveModule
    {
        Spectrum,
        Scopes,
        Meters,
        Traces
    };

    explicit ControlRail (mdsp_ui::UiContext& ui);
    ~ControlRail() override;

    void setControlBinder (AnalyzerPro::ControlBinder& binder);
    void setResetPeaksCallback (std::function<void()> cb);

    /** Attach the user trace-colour store; enables the Traces-module colour swatches. */
    void setTraceColorStore (AnalyzerPro::TraceColorStore* store);
    /** 0 = Compact, 1 = Normal, 2 = Wide. All sections always visible; viewport handles overflow. */
    void setLayoutMode (int mode) { layoutMode_ = juce::jlimit (0, 2, mode); }

    /** Full height required to show all rows (for Viewport content size). */
    int getPreferredHeight() const noexcept;

    /** Callback when active-module content changes preferred height (parent should resized()). */
    std::function<void()> onPreferredHeightChanged;

    void setActiveModule (ActiveModule module);
    ActiveModule getActiveModule() const noexcept { return activeModule_; }

    /** Show the Spectrum rail module where the Analysis Mode controls now live. */
    void expandAnalysisModeSection();

    /** Mode sync: set from parameter/model. 1=FFT, 2=BAND, 3=LOG. */
    void setMode (int modeIndex);
    /** Fired when user changes mode; connect to APVTS like HeaderBar. */
    std::function<void(int)> onModeChanged;

    // Scope Callbacks
    std::function<void(int)> onScopeModeChanged;   // 1=Peak, 2=RMS (phase fan)
    std::function<void(int)> onScopeShapeChanged;  // 1=Lissajous, 2=Scatter (legacy)
    std::function<void(float)> onScopeReleaseChanged; // phase-fan release/decay (ms)

    void paint (juce::Graphics& g) override;
    void resized() override;
    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override;

private:
    int layoutMode_ = 1; // 0 Compact, 1 Normal, 2 Wide
    AnalyzerPro::ControlBinder* controlBinder = nullptr;
    ActiveModule activeModule_ = ActiveModule::Spectrum;

    void triggerResetPeaks();
    void setSelectedModeId (int id) // 1=FFT, 2=BAND, 3=LOG
    {
        fftButton_.setToggleState (id == 1, juce::dontSendNotification);
        bandButton_.setToggleState (id == 2, juce::dontSendNotification);
        logButton_.setToggleState (id == 3, juce::dontSendNotification);
    }
    std::function<void()> onResetPeaks_;

    mdsp_ui::UiContext& ui_;

    // Underlying controls
    juce::ToggleButton holdButton;
    juce::Label releaseTimeLabel_;
    AnalyzerPro::DraggableParamValueLabel releaseTimeValue_;
    juce::Label holdDecayLabel_;
    AnalyzerPro::DraggableParamValueLabel holdDecayValue_;
    juce::ComboBox tiltCombo;
    juce::TextButton resetPeaksButton { "Reset" };
    
    // Scope Controls
    juce::ComboBox scopeModeCombo;
    juce::ComboBox scopeShapeCombo;
    juce::ComboBox scopeInputCombo; // New
    juce::Label scopeReleaseLabel_;                            // Phase-fan release/decay
    AnalyzerPro::DraggableParamValueLabel scopeReleaseValue_;  // draggable ms value (manual mode)
    juce::ToggleButton scopePeakHoldButton;
    
    // Meter Controls
    juce::ComboBox meterInputCombo; // New
    juce::ToggleButton meterPeakHoldButton;
    
    // Active module section headers
    mdsp_ui::SectionHeader spectrumHeader;
    mdsp_ui::SectionHeader scopesHeader;
    mdsp_ui::SectionHeader metersHeader;
    mdsp_ui::SectionHeader tracesHeader;

    // Analysis Mode controls (moved from HeaderBar into rail)
    juce::TextButton fftButton_;
    juce::TextButton bandButton_;
    juce::TextButton logButton_;
    juce::ComboBox fftSizeCombo_;
    juce::ComboBox detailCombo_;
    mdsp_ui::ChoiceRow fftSizeRow_;
    mdsp_ui::ChoiceRow detailRow_;
    
    // Control rows
    mdsp_ui::ToggleRow holdRow;
    mdsp_ui::ChoiceRow tiltRow;
    
    mdsp_ui::ChoiceRow scopeModeRow;
    mdsp_ui::ChoiceRow scopeShapeRow;
    mdsp_ui::ChoiceRow scopeInputRow; // New
    mdsp_ui::ToggleRow scopePeakHoldRow;
    mdsp_ui::ChoiceRow meterInputRow; // New
    mdsp_ui::ToggleRow meterPeakHoldRow;
    
    // Trace Toggles
    juce::ToggleButton showLrButton;
    juce::ToggleButton showMonoButton;
    juce::ToggleButton showLButton;
    juce::ToggleButton showRButton;
    juce::ToggleButton showMidButton;
    juce::ToggleButton showSideButton;
    juce::ToggleButton showPeakButton;
    juce::ToggleButton showRmsButton;
    
    mdsp_ui::ToggleRow showLrRow;
    mdsp_ui::ToggleRow showMonoRow;
    mdsp_ui::ToggleRow showLRow;
    mdsp_ui::ToggleRow showRRow;
    mdsp_ui::ToggleRow showMidRow;
    mdsp_ui::ToggleRow showSideRow;
    mdsp_ui::ToggleRow showPeakRow;
    mdsp_ui::ToggleRow showRmsRow;
    
    // Smoothing
    
    // Smoothing
    juce::ComboBox smoothingCombo;
    mdsp_ui::ChoiceRow smoothingRow;

    // Weighting
    juce::ComboBox weightingCombo;
    mdsp_ui::ChoiceRow weightingRow;

    // Trace colour customisation (Traces module). Index follows TraceId order.
    AnalyzerPro::TraceColorStore* traceColors_ = nullptr; // not owned
    std::array<AnalyzerPro::ColorSwatch, AnalyzerPro::kNumTraceColors> traceSwatches_;
    juce::TextButton resetColorsButton_ { "Reset" };
    juce::TextButton saveColorsDefaultButton_ { "Save Default" };
    void openTraceColourPicker (AnalyzerPro::TraceId id);
    void refreshTraceSwatches();
    void setTraceColorUiVisible (bool visible);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ControlRail)
};
