#pragma once

#include "ControlId.h"
#include "Focus.h"
#include <optional>
#include <unordered_map>

namespace ui_core
{

class FocusManager
{
public:
    void setFocusedControl (std::optional<ControlId> controlId)
    {
        // Clear focus on old widget
        if (focusedControlId.has_value())
        {
            auto it = widgets.find (focusedControlId.value());
            if (it != widgets.end() && it->second != nullptr)
                it->second->setFocused (false);
        }

        // Set focus on new widget
        focusedControlId = controlId;

        if (focusedControlId.has_value())
        {
            auto it = widgets.find (focusedControlId.value());
            if (it != widgets.end() && it->second != nullptr)
                it->second->setFocused (true);
        }
    }

    std::optional<ControlId> getFocusedControl() const
    {
        return focusedControlId;
    }

    void registerWidget (ControlId controlId, Focusable* widget)
    {
        if (widget != nullptr)
            widgets[controlId] = widget;
    }

    void unregisterWidget (ControlId controlId, Focusable* widget)
    {
        auto it = widgets.find (controlId);
        if (it != widgets.end() && it->second == widget)
            widgets.erase (it);
    }

private:
    std::optional<ControlId> focusedControlId;
    std::unordered_map<ControlId, Focusable*> widgets;
};

}
