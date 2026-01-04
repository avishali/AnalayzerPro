#pragma once

#include "ControlId.h"

namespace ui_core
{

enum class HardwareControlType
{
    Encoder,
    Fader,
    Button,
    Touch,
    LED,
    Display
};

struct HardwareControlEvent
{
    ControlId controlId{};
    float normalizedValue{};
    bool isRelative{};
};

struct HardwareControlDescriptor
{
    ControlId controlId{};
    HardwareControlType type{};
    bool supportsTouch{};
    bool supportsLEDFeedback{};
};

}
