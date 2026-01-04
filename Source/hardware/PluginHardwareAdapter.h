#pragma once

#include <ui_core/UiCore.h>
#include <algorithm>

//==============================================================================
/**
    Hardware input adapter that routes events to BindingRegistry.
*/
class PluginHardwareAdapter : public ui_core::HardwareInputAdapter
{
public:
    explicit PluginHardwareAdapter (ui_core::BindingRegistry& registry);
    ~PluginHardwareAdapter() override = default;

    void processEvent (const ui_core::HardwareControlEvent& event) override;

private:
    ui_core::BindingRegistry& bindingRegistry;
};
