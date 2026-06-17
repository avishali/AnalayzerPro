#include "DevLookControlsComponent.h"

#if ANALYZERPRO_DEV_LOOK_PANEL

namespace AnalyzerPro
{

namespace
{
constexpr int kRowHeight   = 52;
constexpr int kGroupPad    = 10;
constexpr int kGroupHeader = 22;
constexpr int kCopyBarH    = 34;
constexpr int kViewportPad = 8;
}

DevLookControlsComponent::DevLookControlsComponent (metal::MetalLookTunables& tunables,
                                                    std::function<void()> onChanged)
    : tunables_ (tunables),
      onChanged_ (std::move (onChanged))
{
    addAndMakeVisible (viewport_);
    viewport_.setViewedComponent (&scrollContent_, false);
    viewport_.setScrollBarsShown (true, false);
    viewport_.setScrollBarThickness (12);

    copyButton_.onClick = [this]
    {
        juce::SystemClipboard::copyTextToClipboard (formatTunablesForClipboard (tunables_));
    };
    addAndMakeVisible (copyButton_);

    const auto notify = [this] (float value)
    {
        juce::ignoreUnused (value);
        if (onChanged_)
            onChanged_();
    };

    auto addGroup = [this, notify] (const juce::String& title, auto&& addFields)
    {
        auto* group = new juce::GroupComponent (title, title);
        group->setComponentID (title);
        scrollContent_.addAndMakeVisible (group);
        addFields();
    };

    addGroup ("Traces", [this, notify]
    {
        addRow (scrollContent_, "Glow mult (emph)", 1.0, 8.0, 0.05, 2,
                [this] { return tunables_.glowMultEmph; },
                [this, notify] (float v) { tunables_.glowMultEmph = v; notify (v); });
        addRow (scrollContent_, "Glow mult (norm)", 1.0, 8.0, 0.05, 2,
                [this] { return tunables_.glowMultNorm; },
                [this, notify] (float v) { tunables_.glowMultNorm = v; notify (v); });
        addRow (scrollContent_, "Glow alpha (emph)", 0.0, 1.0, 0.01, 2,
                [this] { return tunables_.glowAlphaEmph; },
                [this, notify] (float v) { tunables_.glowAlphaEmph = v; notify (v); });
        addRow (scrollContent_, "Glow alpha (norm)", 0.0, 1.0, 0.01, 2,
                [this] { return tunables_.glowAlphaNorm; },
                [this, notify] (float v) { tunables_.glowAlphaNorm = v; notify (v); });
        addRow (scrollContent_, "Shadow mult", 1.0, 8.0, 0.05, 2,
                [this] { return tunables_.shadowMult; },
                [this, notify] (float v) { tunables_.shadowMult = v; notify (v); });
        addRow (scrollContent_, "Shadow alpha", 0.0, 1.0, 0.01, 2,
                [this] { return tunables_.shadowAlpha; },
                [this, notify] (float v) { tunables_.shadowAlpha = v; notify (v); });
        addRow (scrollContent_, "Core alpha (emph)", 0.0, 1.0, 0.01, 2,
                [this] { return tunables_.coreAlphaEmph; },
                [this, notify] (float v) { tunables_.coreAlphaEmph = v; notify (v); });
        addRow (scrollContent_, "Core alpha (norm)", 0.0, 1.0, 0.01, 2,
                [this] { return tunables_.coreAlphaNorm; },
                [this, notify] (float v) { tunables_.coreAlphaNorm = v; notify (v); });
        addRow (scrollContent_, "Highlight mult", 0.0, 2.0, 0.05, 2,
                [this] { return tunables_.hiMult; },
                [this, notify] (float v) { tunables_.hiMult = v; notify (v); });
        addRow (scrollContent_, "Highlight alpha", 0.0, 1.0, 0.01, 2,
                [this] { return tunables_.hiAlpha; },
                [this, notify] (float v) { tunables_.hiAlpha = v; notify (v); });
        addRow (scrollContent_, "Highlight brighten", 0.0, 1.0, 0.01, 2,
                [this] { return tunables_.hiBrighten; },
                [this, notify] (float v) { tunables_.hiBrighten = v; notify (v); });
    });

    addGroup ("Fills", [this, notify]
    {
        addRow (scrollContent_, "Peak fill top", 0.0, 1.0, 0.01, 2,
                [this] { return tunables_.peakFillTop; },
                [this, notify] (float v) { tunables_.peakFillTop = v; notify (v); });
        addRow (scrollContent_, "Peak fill bottom", 0.0, 1.0, 0.01, 2,
                [this] { return tunables_.peakFillBot; },
                [this, notify] (float v) { tunables_.peakFillBot = v; notify (v); });
        addRow (scrollContent_, "Hold fill top", 0.0, 1.0, 0.01, 2,
                [this] { return tunables_.holdFillTop; },
                [this, notify] (float v) { tunables_.holdFillTop = v; notify (v); });
        addRow (scrollContent_, "Hold fill bottom", 0.0, 1.0, 0.01, 2,
                [this] { return tunables_.holdFillBot; },
                [this, notify] (float v) { tunables_.holdFillBot = v; notify (v); });
    });

    addGroup ("Meters", [this, notify]
    {
        addRow (scrollContent_, "Glow margin (px)", 0.0, 24.0, 0.5, 1,
                [this] { return tunables_.meterGlowMargin; },
                [this, notify] (float v) { tunables_.meterGlowMargin = v; notify (v); });
        addRow (scrollContent_, "Halo top alpha", 0.0, 1.0, 0.01, 2,
                [this] { return tunables_.meterHaloTop; },
                [this, notify] (float v) { tunables_.meterHaloTop = v; notify (v); });
        addRow (scrollContent_, "Halo bottom alpha", 0.0, 1.0, 0.01, 2,
                [this] { return tunables_.meterHaloBot; },
                [this, notify] (float v) { tunables_.meterHaloBot = v; notify (v); });
        addRow (scrollContent_, "Cap glow alpha", 0.0, 1.0, 0.01, 2,
                [this] { return tunables_.meterCapGlowAlpha; },
                [this, notify] (float v) { tunables_.meterCapGlowAlpha = v; notify (v); });
    });

    addGroup ("Phase fan", [this, notify]
    {
        addRow (scrollContent_, "Fill glow scale", 1.0, 2.0, 0.01, 2,
                [this] { return tunables_.phaseFanGlowScale; },
                [this, notify] (float v) { tunables_.phaseFanGlowScale = v; notify (v); });
        addRow (scrollContent_, "Fill glow alpha", 0.0, 1.0, 0.01, 2,
                [this] { return tunables_.phaseFanGlowAlpha; },
                [this, notify] (float v) { tunables_.phaseFanGlowAlpha = v; notify (v); });
        addRow (scrollContent_, "Line width (px)", 0.5, 6.0, 0.1, 1,
                [this] { return tunables_.phaseFanLineWidth; },
                [this, notify] (float v) { tunables_.phaseFanLineWidth = v; notify (v); });
        addRow (scrollContent_, "Peak width (px)", 0.5, 6.0, 0.1, 1,
                [this] { return tunables_.phaseFanPeakWidth; },
                [this, notify] (float v) { tunables_.phaseFanPeakWidth = v; notify (v); });
        addRow (scrollContent_, "Line glow mult", 1.0, 8.0, 0.05, 2,
                [this] { return tunables_.phaseFanLineGlowMult; },
                [this, notify] (float v) { tunables_.phaseFanLineGlowMult = v; notify (v); });
        addRow (scrollContent_, "Line glow alpha", 0.0, 1.0, 0.01, 2,
                [this] { return tunables_.phaseFanLineGlowAlpha; },
                [this, notify] (float v) { tunables_.phaseFanLineGlowAlpha = v; notify (v); });
        addRow (scrollContent_, "Line core alpha", 0.0, 1.0, 0.01, 2,
                [this] { return tunables_.phaseFanLineCoreAlpha; },
                [this, notify] (float v) { tunables_.phaseFanLineCoreAlpha = v; notify (v); });
    });

    addGroup ("Gonio", [this, notify]
    {
        addRow (scrollContent_, "Glow mult", 1.0, 8.0, 0.05, 2,
                [this] { return tunables_.gonioGlowMult; },
                [this, notify] (float v) { tunables_.gonioGlowMult = v; notify (v); });
        addRow (scrollContent_, "Glow alpha", 0.0, 1.0, 0.01, 2,
                [this] { return tunables_.gonioGlowAlpha; },
                [this, notify] (float v) { tunables_.gonioGlowAlpha = v; notify (v); });
    });
}

void DevLookControlsComponent::addRow (juce::Component& parent,
                                       const juce::String& text,
                                       double minValue,
                                       double maxValue,
                                       double step,
                                       int decimals,
                                       std::function<float()> getter,
                                       std::function<void (float)> setter)
{
    auto row = std::make_unique<SliderRow>();
    row->label.setText (text, juce::dontSendNotification);
    row->label.setJustificationType (juce::Justification::centredLeft);
    row->slider.setSliderStyle (juce::Slider::LinearHorizontal);
    row->slider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 64, 20);
    row->slider.setRange (minValue, maxValue, step);
    row->slider.setNumDecimalPlacesToDisplay (decimals);
    row->slider.setValue (getter(), juce::dontSendNotification);
    row->slider.onValueChange = [getter, setter, slider = &row->slider]
    {
        setter (static_cast<float> (slider->getValue()));
    };

    parent.addAndMakeVisible (row->label);
    parent.addAndMakeVisible (row->slider);
    rows_.push_back (std::move (row));
}

void DevLookControlsComponent::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff1e1e1e));
}

void DevLookControlsComponent::resized()
{
    auto area = getLocalBounds().reduced (kViewportPad);
    copyButton_.setBounds (area.removeFromBottom (kCopyBarH));
    viewport_.setBounds (area);

    const int width = juce::jmax (280, viewport_.getWidth());
    const int rowsPerGroup[] = { 11, 4, 4, 7, 2 };
    const char* groupTitles[] = { "Traces", "Fills", "Meters", "Phase fan", "Gonio" };
    int y = kGroupPad;
    size_t rowIndex = 0;

    for (int g = 0; g < 5; ++g)
    {
        const int groupRows = rowsPerGroup[g];
        const int groupHeight = kGroupHeader + groupRows * kRowHeight + kGroupPad;
        if (auto* group = scrollContent_.findChildWithID (groupTitles[g]))
            group->setBounds (0, y, width, groupHeight);

        for (int r = 0; r < groupRows && rowIndex < rows_.size(); ++r, ++rowIndex)
        {
            auto& row = *rows_[rowIndex];
            const int rowY = y + kGroupHeader + r * kRowHeight;
            row.label.setBounds (12, rowY + 2, width - 24, 18);
            row.slider.setBounds (12, rowY + 22, width - 24, 24);
        }

        y += groupHeight + kGroupPad;
    }

    contentHeight_ = y + kGroupPad;
    scrollContent_.setSize (width, contentHeight_);
}

juce::String DevLookControlsComponent::formatTunablesForClipboard (const metal::MetalLookTunables& t)
{
    return juce::String::formatted (
        "MetalLookTunables {\n"
        "    // analyzer trace stroke\n"
        "    %.2ff, %.2ff,  // glowMultEmph, glowMultNorm\n"
        "    %.2ff, %.2ff,  // glowAlphaEmph, glowAlphaNorm\n"
        "    %.2ff, %.2ff,  // shadowMult, shadowAlpha\n"
        "    %.2ff, %.2ff,  // coreAlphaEmph, coreAlphaNorm\n"
        "    %.2ff, %.2ff, %.2ff,  // hiMult, hiAlpha, hiBrighten\n"
        "    // peak / hold fills\n"
        "    %.2ff, %.2ff,  // peakFillTop, peakFillBot\n"
        "    %.2ff, %.2ff,  // holdFillTop, holdFillBot\n"
        "    // meters\n"
        "    %.1ff, %.2ff, %.2ff, %.2ff,  // meterGlowMargin, meterHaloTop, meterHaloBot, meterCapGlowAlpha\n"
        "    // phase-fan\n"
        "    %.2ff, %.2ff,  // phaseFanGlowScale, phaseFanGlowAlpha\n"
        "    %.2ff, %.2ff, %.2ff, %.2ff, %.2ff,  // lineWidth, peakWidth, lineGlowMult, lineGlowAlpha, lineCoreAlpha\n"
        "    // goniometer\n"
        "    %.2ff, %.2ff,  // gonioGlowMult, gonioGlowAlpha\n"
        "};",
        t.glowMultEmph, t.glowMultNorm,
        t.glowAlphaEmph, t.glowAlphaNorm,
        t.shadowMult, t.shadowAlpha,
        t.coreAlphaEmph, t.coreAlphaNorm,
        t.hiMult, t.hiAlpha, t.hiBrighten,
        t.peakFillTop, t.peakFillBot,
        t.holdFillTop, t.holdFillBot,
        t.meterGlowMargin, t.meterHaloTop, t.meterHaloBot, t.meterCapGlowAlpha,
        t.phaseFanGlowScale, t.phaseFanGlowAlpha,
        t.phaseFanLineWidth, t.phaseFanPeakWidth, t.phaseFanLineGlowMult, t.phaseFanLineGlowAlpha, t.phaseFanLineCoreAlpha,
        t.gonioGlowMult, t.gonioGlowAlpha);
}

} // namespace AnalyzerPro

#endif
