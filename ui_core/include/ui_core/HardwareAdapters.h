#pragma once

#include "ControlId.h"
#include "HardwareContract.h"

namespace ui_core
{

class HardwareInputAdapter
{
public:
    virtual ~HardwareInputAdapter() = default;
    virtual void processEvent (const HardwareControlEvent& event) = 0;
};

class HardwareOutputAdapter
{
public:
    virtual ~HardwareOutputAdapter() = default;
    virtual void setLEDValue (ControlId controlId, float normalized) = 0;
    virtual void setFocus (ControlId controlId, bool focused) = 0;
};

}
