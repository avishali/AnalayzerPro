#pragma once

#include "ParameterBinding.h"
#include <unordered_map>

namespace ui_core
{

class BindingRegistry
{
public:
    void add (ParameterBinding binding)
    {
        bindings[binding.controlId] = std::move (binding);
    }

    ParameterBinding* find (ControlId id)
    {
        auto it = bindings.find (id);
        return it != bindings.end() ? &it->second : nullptr;
    }

    const ParameterBinding* find (ControlId id) const
    {
        auto it = bindings.find (id);
        return it != bindings.end() ? &it->second : nullptr;
    }

    void clear()
    {
        bindings.clear();
    }

private:
    std::unordered_map<ControlId, ParameterBinding> bindings;
};

}
