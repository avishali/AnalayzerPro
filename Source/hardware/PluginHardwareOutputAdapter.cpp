#include "PluginHardwareOutputAdapter.h"
#include <juce_core/juce_core.h>

//==============================================================================
void PluginHardwareOutputAdapter::setLEDValue (ui_core::ControlId controlId, float normalized)
{
    DBG ("HW OUT LED id=" + juce::String (controlId) + " value=" + juce::String (normalized));
}

void PluginHardwareOutputAdapter::setFocus (ui_core::ControlId controlId, bool focused)
{
    DBG ("HW OUT FOCUS id=" + juce::String (controlId) + " focused=" + juce::String (focused ? 1 : 0));
}
