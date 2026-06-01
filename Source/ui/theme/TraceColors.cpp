#include "TraceColors.h"

namespace AnalyzerPro
{

namespace
{
const juce::Identifier kTraceColorsType ("TraceColors");
}

//==============================================================================
TraceColorStore::TraceColorStore (juce::AudioProcessorValueTreeState& apvts)
    : apvts_ (apvts)
{
}

const TraceColorDef& TraceColorStore::def (TraceId id)
{
    return traceColorDefs()[static_cast<size_t> (id)];
}

juce::Colour TraceColorStore::defaultColour (TraceId id)
{
    return juce::Colour (def (id).defaultArgb);
}

juce::ValueTree TraceColorStore::colorsTree() const
{
    return apvts_.state.getOrCreateChildWithName (kTraceColorsType, nullptr);
}

juce::Colour TraceColorStore::get (TraceId id) const
{
    auto tree = colorsTree();
    const juce::Identifier key (def (id).key);
    if (tree.hasProperty (key))
        return juce::Colour (static_cast<juce::uint32> (static_cast<juce::int64> (tree.getProperty (key))));
    return defaultColour (id);
}

void TraceColorStore::set (TraceId id, juce::Colour c)
{
    auto tree = colorsTree();
    tree.setProperty (juce::Identifier (def (id).key),
                      static_cast<juce::int64> (c.getARGB()),
                      nullptr);
    if (onChanged)
        onChanged();
}

void TraceColorStore::resetToDefaults()
{
    auto tree = colorsTree();
    for (const auto& d : traceColorDefs())
        tree.removeProperty (juce::Identifier (d.key), nullptr);
    if (onChanged)
        onChanged();
}

//==============================================================================
juce::File TraceColorStore::userDefaultFile()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
               .getChildFile ("MelechDSP")
               .getChildFile ("AnalyzerPro")
               .getChildFile ("trace_colors_default.xml");
}

void TraceColorStore::saveAsUserDefault() const
{
    juce::ValueTree out (kTraceColorsType);
    for (const auto& d : traceColorDefs())
        out.setProperty (juce::Identifier (d.key),
                         static_cast<juce::int64> (get (d.id).getARGB()),
                         nullptr);

    auto file = userDefaultFile();
    file.getParentDirectory().createDirectory();
    if (auto xml = out.createXml())
        xml->writeTo (file);
}

void TraceColorStore::loadUserDefaultIntoEmpty()
{
    auto tree = colorsTree();
    // Only seed when the session/preset state hasn't already set colours.
    const bool hasAny = std::any_of (traceColorDefs().begin(), traceColorDefs().end(),
                                     [&] (const TraceColorDef& d)
                                     { return tree.hasProperty (juce::Identifier (d.key)); });
    if (hasAny)
        return;

    auto file = userDefaultFile();
    if (! file.existsAsFile())
        return;

    if (auto xml = juce::XmlDocument::parse (file))
    {
        auto loaded = juce::ValueTree::fromXml (*xml);
        if (loaded.isValid())
            for (const auto& d : traceColorDefs())
            {
                const juce::Identifier key (d.key);
                if (loaded.hasProperty (key))
                    tree.setProperty (key, loaded.getProperty (key), nullptr);
            }
    }

    if (onChanged)
        onChanged();
}

} // namespace AnalyzerPro
