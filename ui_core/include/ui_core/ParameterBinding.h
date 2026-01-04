#pragma once

#include "ControlId.h"
#include <functional>

namespace ui_core
{

struct ParameterBinding
{
    ControlId controlId{};
    std::function<void(float)> setNormalized;
    std::function<float()> getNormalized;
    std::function<float(float)> toNative;
    std::function<float(float)> toNormalized;

    void set (float normalizedValue)
    {
        if (setNormalized)
        {
            if (toNative)
                setNormalized (toNative (normalizedValue));
            else
                setNormalized (normalizedValue);
        }
    }

    float get() const
    {
        if (!getNormalized)
            return 0.0f;

        float nativeValue = getNormalized();
        if (toNormalized)
            return toNormalized (nativeValue);
        else
            return nativeValue;
    }
};

inline ParameterBinding makeBinding (ControlId id,
                                     std::function<float()> getter,
                                     std::function<void(float)> setter)
{
    ParameterBinding binding;
    binding.controlId = id;
    binding.getNormalized = std::move (getter);
    binding.setNormalized = std::move (setter);
    return binding;
}

inline ParameterBinding makeMappedBinding (ControlId id,
                                           std::function<float()> getterNative,
                                           std::function<void(float)> setterNative,
                                           std::function<float(float)> toNative,
                                           std::function<float(float)> toNormalized)
{
    ParameterBinding binding;
    binding.controlId = id;
    binding.getNormalized = std::move (getterNative);
    binding.setNormalized = std::move (setterNative);
    binding.toNative = std::move (toNative);
    binding.toNormalized = std::move (toNormalized);
    return binding;
}

}
