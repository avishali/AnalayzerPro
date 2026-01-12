#pragma once

#include <vector>
#include <map>

namespace ui_core
{
    // =========================================================================
    // TYPES
    // =========================================================================
    using ControlId = int;

    struct HardwareControlEvent
    {
        ControlId controlId = 0;
        bool isRelative = false;
        float normalizedValue = 0.0f;
    };

    // =========================================================================
    // INTERFACES
    // =========================================================================
    class Binding
    {
    public:
        virtual ~Binding() = default;
        virtual float get() const = 0;
        virtual void set (float value) = 0;
    };

    class BindingRegistry
    {
    public:
        virtual ~BindingRegistry() = default;
        virtual Binding* find (ControlId id) = 0;
    };

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

    // Placeholder initialization
    inline void init() {}
}
