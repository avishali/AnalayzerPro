#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <mdsp_ui/UiContext.h>
#include "../../control/ControlBinder.h"
#include <mdsp_ui/controls/SectionHeader.h>
#include <mdsp_ui/controls/ChoiceRow.h>
#include <mdsp_ui/controls/ToggleRow.h>
#include "DraggableParamValueLabel.h"
#include <mdsp_ui/controls/CollapsibleSection.h>
#include <functional>

//==============================================================================
/**
    Control rail component for right-side controls.
    Contains placeholder sections for control groups.
*/
class ControlRail : public juce::Component
{
public:
    explicit ControlRail (mdsp_ui::UiContext& ui);
    ~ControlRail() override;

    void setControlBinder (AnalyzerPro::ControlBinder& binder);
    void setResetPeaksCallback (std::function<void()> cb);
    /** 0 = Compact, 1 = Normal, 2 = Wide. All sections always visible; viewport handles overflow. */
    void setLayoutMode (int mode) { layoutMode_ = juce::jlimit (0, 2, mode); }

    /** Full height required to show all rows (for Viewport content size). */
    int getPreferredHeight() const noexcept;

    /** Callback when section expand/collapse changes preferred height (parent should resized()). */
    std::function<void()> onPreferredHeightChanged;

    /** Expand the Analysis Mode section (e.g. when HeaderBar "Mode…" is clicked). */
    void expandAnalysisModeSection();

    /** Mode sync: set from parameter/model. 1=FFT, 2=BAND, 3=LOG. */
    void setMode (int modeIndex);
    /** Fired when user changes mode; connect to APVTS like HeaderBar. */
    std::function<void(int)> onModeChanged;

    // Scope Callbacks
    std::function<void(int)> onScopeModeChanged;  // 1=Peak, 2=RMS
    std::function<void(int)> onScopeShapeChanged; // 1=Lissajous, 2=Scatter

    void paint (juce::Graphics& g) override;
    void resized() override;
    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override;

private:
    int layoutMode_ = 1; // 0 Compact, 1 Normal, 2 Wide
    AnalyzerPro::ControlBinder* controlBinder = nullptr;

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
    juce::ComboBox tiltCombo;
    juce::TextButton resetPeaksButton { "Reset" };
    
    // Scope Controls
    juce::ComboBox scopeModeCombo;
    juce::ComboBox scopeShapeCombo;
    juce::ComboBox scopeInputCombo; // New
    juce::ToggleButton scopePeakHoldButton;
    
    // Meter Controls
    juce::ComboBox meterInputCombo; // New
    juce::ToggleButton meterPeakHoldButton;
    
    // Section headers
    mdsp_ui::SectionHeader navigateHeader;
    mdsp_ui::SectionHeader analyzerHeader;
    mdsp_ui::SectionHeader displayHeader;
    mdsp_ui::SectionHeader metersHeader;

    // Collapsible sections (Scopes, Traces, Analysis Mode — default collapsed)
    mdsp_ui::CollapsibleSection scopesSection_;
    mdsp_ui::CollapsibleSection tracesSection_;
    mdsp_ui::CollapsibleSection analysisModeSection_;

    // Analysis Mode controls (moved from HeaderBar into rail)
    juce::TextButton fftButton_;
    juce::TextButton bandButton_;
    juce::TextButton logButton_;
    juce::ComboBox fftSizeCombo_;
    mdsp_ui::ChoiceRow fftSizeRow_;
    
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
    juce::ToggleButton showRmsButton;
    
    mdsp_ui::ToggleRow showLrRow;
    mdsp_ui::ToggleRow showMonoRow;
    mdsp_ui::ToggleRow showLRow;
    mdsp_ui::ToggleRow showRRow;
    mdsp_ui::ToggleRow showMidRow;
    mdsp_ui::ToggleRow showSideRow;
    mdsp_ui::ToggleRow showRmsRow;
    
    // Smoothing
    
    // Smoothing
    juce::ComboBox smoothingCombo;
    mdsp_ui::ChoiceRow smoothingRow;

    // Weighting
    juce::ComboBox weightingCombo;
    mdsp_ui::ChoiceRow weightingRow;
    
    // Navigate section (placeholder)
    juce::Label placeholderLabel1;
    
    // Display section (placeholder)
    juce::Label placeholderLabel3;
    
    // Meters section (placeholder)
    juce::Label placeholderLabel4;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ControlRail)
};
