#include "PluginHardwareAdapter.h"

//==============================================================================
PluginHardwareAdapter::PluginHardwareAdapter (ui_core::BindingRegistry& registry)
    : bindingRegistry (registry)
{
}

void PluginHardwareAdapter::processEvent (const ui_core::HardwareControlEvent& event)
{
    if (auto* binding = bindingRegistry.find (event.controlId))
    {
        if (event.isRelative)
        {
            // Relative event: add delta to current normalized value (0..1)
            float currentNormalized = binding->get();
            float nextNormalized = std::clamp (currentNormalized + event.normalizedValue, 0.0f, 1.0f);
            binding->set (nextNormalized);
        }
        else
        {
            // Absolute event: set normalized value directly (0..1)
            binding->set (event.normalizedValue);
        }
    }
}
