#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <mdsp_ui/UiContext.h>
#include <mdsp_ui/controls/ChoiceRow.h>
#include <mdsp_ui/controls/ToggleRow.h>
#include "DraggableParamValueLabel.h"
#include "../../control/ControlBinder.h"
#include <functional>
#include <memory>

/**
    Floating settings panel shown as a CallOutBox below header dropdown buttons.
    Each instance covers one module section (Spectrum / Scopes / Meters / Traces).
    Owns its own APVTS attachments; cleaned up automatically on destruction.
*/
class SettingsPopupPanel : public juce::Component
{
public:
    enum class Section { Spectrum, Scopes, Meters, Traces };

    SettingsPopupPanel (Section section,
                        mdsp_ui::UiContext& ui,
                        juce::AudioProcessorValueTreeState* apvts);
    ~SettingsPopupPanel() override;

    // ── Callbacks for non-APVTS controls ────────────────────────────────────
    std::function<void (int)> onModeChanged;       // 1=FFT 2=Band 3=Log
    std::function<void()>     onResetPeaks;
    std::function<void (int)> onScopeModeChanged;  // 1=Peak 2=RMS
    std::function<void (int)> onScopeShapeChanged; // 1=Basic 2=PAZ

    // Set before showing so non-APVTS controls start at the right state
    void setCurrentMode (int modeId);      // 1=FFT 2=Band 3=Log
    void setCurrentScopeMode (int id);
    void setCurrentScopeShape (int id);

    static constexpr int kWidth = 240;
    int getPreferredHeight() const;

    void paint  (juce::Graphics& g) override;
    void resized() override;

private:
    Section section_;
    mdsp_ui::UiContext& ui_;
    std::unique_ptr<AnalyzerPro::ControlBinder> binder_;

    // ── Controls – declared before rows so initializer-list refs are valid ──

    // Spectrum
    juce::TextButton     fftBtn_, bandBtn_, logBtn_, resetBtn_;
    juce::ComboBox       fftSizeCombo_, tiltCombo_, smoothingCombo_, weightingCombo_;
    juce::ToggleButton   holdBtn_;
    AnalyzerPro::DraggableParamValueLabel releaseTimeValue_;
    juce::Label          releaseTimeLabel_;

    // Scopes
    juce::ComboBox       scopeModeCombo_, scopeShapeCombo_, scopeInputCombo_;
    juce::ToggleButton   scopeHoldBtn_;

    // Meters
    juce::ComboBox       meterInputCombo_;
    juce::ToggleButton   meterHoldBtn_;

    // Traces
    juce::ToggleButton   lrBtn_, monoBtn_, lBtn_, rBtn_, midBtn_, sideBtn_, rmsBtn_;

    // Scrollable container for Traces section
    juce::Viewport       tracesViewport_;
    juce::Component      tracesContainer_;

    // ── SDK rows (must be after the control members they reference) ─────────
    mdsp_ui::ChoiceRow   fftSizeRow_, tiltRow_, smoothingRow_, weightingRow_;
    mdsp_ui::ToggleRow   holdRow_;
    mdsp_ui::ChoiceRow   scopeModeRow_, scopeShapeRow_, scopeInputRow_;
    mdsp_ui::ToggleRow   scopeHoldRow_;
    mdsp_ui::ChoiceRow   meterInputRow_;
    mdsp_ui::ToggleRow   meterHoldRow_;
    mdsp_ui::ToggleRow   lrRow_, monoRow_, lRow_, rRow_, midRow_, sideRow_, rmsRow_;

    void initSpectrum (juce::AudioProcessorValueTreeState* apvts);
    void initScopes   (juce::AudioProcessorValueTreeState* apvts);
    void initMeters   (juce::AudioProcessorValueTreeState* apvts);
    void initTraces   (juce::AudioProcessorValueTreeState* apvts);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SettingsPopupPanel)
};
