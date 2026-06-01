#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_gui_extra/juce_gui_extra.h>          // juce::ColourSelector
#include <juce_data_structures/juce_data_structures.h>
#include <juce_audio_processors/juce_audio_processors.h> // juce::AudioProcessorValueTreeState
#include <functional>
#include <array>

namespace AnalyzerPro
{

//==============================================================================
/** The eight user-customisable spectrum traces. Order is stable (used as index). */
enum class TraceId { LR = 0, Mono, L, R, Mid, Side, Rms, Peak };
inline constexpr int kNumTraceColors = 8;

struct TraceColorDef
{
    TraceId          id;
    const char*      key;          // ValueTree property key
    const char*      name;         // display name
    juce::uint32     defaultArgb;  // default colour (matches current hardcoded look)
};

/** Single source of truth for trace ids, keys, names and default colours. */
inline const std::array<TraceColorDef, kNumTraceColors>& traceColorDefs()
{
    static const std::array<TraceColorDef, kNumTraceColors> defs = {{
        { TraceId::LR,   "lr",   "Stereo", 0xffb388ff },
        { TraceId::Mono, "mono", "Mono",   0xff29b6f6 },
        { TraceId::L,    "l",    "Left",   0xff4caf50 },
        { TraceId::R,    "r",    "Right",  0xfff44336 },
        { TraceId::Mid,  "mid",  "Mid",    0xff00bcd4 },
        { TraceId::Side, "side", "Side",   0xffe91e63 },
        { TraceId::Rms,  "rms",  "RMS",    0xffadd8e6 }, // lightblue (current override)
        { TraceId::Peak, "peak", "Peak",   0xffffff33 }, // yellow (current override)
    }};
    return defs;
}

//==============================================================================
/**
    Stores per-trace colours in a "TraceColors" child of the APVTS state tree.

    Because the colours live inside apvts.state, they are persisted automatically
    with the DAW session and inside saved presets (PresetManager serialises the
    APVTS tree). Values are read live via get(), so a preset/state reload is
    reflected without any listener wiring. A separate user-default file backs the
    "Save as default" / new-instance behaviour.
*/
class TraceColorStore
{
public:
    explicit TraceColorStore (juce::AudioProcessorValueTreeState& apvts);

    juce::Colour get (TraceId id) const;
    void         set (TraceId id, juce::Colour c);

    void resetToDefaults();           // clear overrides → built-in defaults
    void saveAsUserDefault() const;   // persist current colours as the global default
    void loadUserDefaultIntoEmpty();  // on first construction, seed from user default if state has none

    static juce::Colour defaultColour (TraceId id);

    /** Optional: fired after set()/reset()/load so UI swatches can refresh. */
    std::function<void()> onChanged;

private:
    juce::ValueTree colorsTree() const;            // get-or-create <TraceColors> child
    static const TraceColorDef& def (TraceId id);
    static juce::File userDefaultFile();

    juce::AudioProcessorValueTreeState& apvts_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TraceColorStore)
};

//==============================================================================
/** A juce::ColourSelector that reports live colour changes via a std::function. */
class LiveColourSelector : public juce::ColourSelector,
                           private juce::ChangeListener
{
public:
    explicit LiveColourSelector (int flags)
        : juce::ColourSelector (flags)
    {
        addChangeListener (this);
    }

    ~LiveColourSelector() override { removeChangeListener (this); }

    std::function<void (juce::Colour)> onColour;

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override
    {
        if (onColour)
            onColour (getCurrentColour());
    }
};

//==============================================================================
/** Small clickable colour chip; shows a trace colour and opens the picker on click. */
class ColorSwatch : public juce::Component,
                    public juce::SettableTooltipClient
{
public:
    ColorSwatch() { setMouseCursor (juce::MouseCursor::PointingHandCursor); }

    void setSwatchColour (juce::Colour c) { colour_ = c; repaint(); }
    juce::Colour getSwatchColour() const  { return colour_; }

    std::function<void()> onClick;

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced (1.0f);
        g.setColour (colour_);
        g.fillRoundedRectangle (r, 3.0f);
        g.setColour (juce::Colours::white.withAlpha (0.35f));
        g.drawRoundedRectangle (r, 3.0f, 1.0f);
    }

    void mouseUp (const juce::MouseEvent& e) override
    {
        if (onClick && getLocalBounds().contains (e.getPosition()))
            onClick();
    }

private:
    juce::Colour colour_ { juce::Colours::grey };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ColorSwatch)
};

} // namespace AnalyzerPro
