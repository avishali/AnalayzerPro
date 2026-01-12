#include "TooltipOverlayComponent.h"

namespace mdsp_ui
{

TooltipOverlayComponent::TooltipOverlayComponent (UiContext& ui)
    : ui_ (ui)
{
    setAlwaysOnTop (true);
    setInterceptsMouseClicks (false, false);
}

void TooltipOverlayComponent::updateSpec (const TooltipSpec& spec)
{
    currentSpec_ = spec;
    lastValueString_.clear();
    refreshLayout();
}

void TooltipOverlayComponent::updateValue()
{
    if (currentSpec_.valueProvider)
    {
        auto newVal = currentSpec_.valueProvider();
        if (newVal != lastValueString_)
        {
            lastValueString_ = newVal;
            refreshLayout();
            repaint();
        }
    }
}

void TooltipOverlayComponent::refreshLayout()
{
    const auto& theme = ui_.theme();
    const auto& type  = ui_.type();
    
    // Create Layouts
    juce::AttributedString sTitle;
    sTitle.append (currentSpec_.title + " ", type.sectionTitleFont().withHeight (13.0f), theme.text);
    
    if (currentSpec_.valueProvider)
    {
         // Append value in Accent Color
         sTitle.append (lastValueString_, type.sectionTitleFont().withHeight (13.0f), theme.accent);
    }
    
    titleLayout_.createLayout (sTitle, 300.0f); // Max width

    if (currentSpec_.description.isNotEmpty())
    {
        juce::AttributedString sDesc;
        sDesc.append (currentSpec_.description, type.labelFont().withHeight (12.0f), theme.textMuted);
        descLayout_.createLayout (sDesc, 300.0f);
    }
    else
    {
        descLayout_ = juce::TextLayout(); // Clear
    }
    
    // Calculate Size
    const float pad = 8.0f;
    float w = titleLayout_.getWidth();
    float h = titleLayout_.getHeight();
    
    if (currentSpec_.description.isNotEmpty())
    {
        w = juce::jmax (w, descLayout_.getWidth());
        h += descLayout_.getHeight() + 4.0f; // Gap
    }
    
    setSize (static_cast<int> (w + pad * 2.0f), static_cast<int> (h + pad * 2.0f));
}

void TooltipOverlayComponent::paint (juce::Graphics& g)
{
    const auto& theme = ui_.theme();
    auto b = getLocalBounds().toFloat();
    
    // Draw Bubble
    g.setColour (theme.panel.withAlpha (0.98f));
    g.fillRoundedRectangle (b, 4.0f);
    g.setColour (theme.gridMajor.withAlpha (0.5f)); // Use gridMajor as outline proxy
    g.drawRoundedRectangle (b, 4.0f, 1.0f);
    
    // Draw Text
    const float pad = 8.0f;
    float y = pad;
    
    titleLayout_.draw (g, { pad, y, titleLayout_.getWidth(), titleLayout_.getHeight() });
    
    if (currentSpec_.description.isNotEmpty())
    {
        y += titleLayout_.getHeight() + 4.0f;
        descLayout_.draw (g, { pad, y, descLayout_.getWidth(), descLayout_.getHeight() });
    }
}

void TooltipOverlayComponent::resized()
{
}

} // namespace mdsp_ui
