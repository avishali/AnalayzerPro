#pragma once

namespace ui_core
{

class Focusable
{
public:
    virtual ~Focusable() = default;
    virtual void setFocused (bool focused) = 0;
};

struct FocusState
{
    bool focused = false;
    bool hovered = false;
    bool pressed = false;
};

}
