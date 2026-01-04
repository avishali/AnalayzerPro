#pragma once

#include <ui_core/UiCore.h>

//==============================================================================
/**
    Hardware output adapter that sends feedback to hardware devices.
    Currently uses DBG logging as the output backend.
*/
class PluginHardwareOutputAdapter : public ui_core::HardwareOutputAdapter
{
public:
    PluginHardwareOutputAdapter() = default;
    ~PluginHardwareOutputAdapter() override = default;

    void setLEDValue (ui_core::ControlId controlId, float normalized) override;
    void setFocus (ui_core::ControlId controlId, bool focused) override;
};
