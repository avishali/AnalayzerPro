# Control IDs & Hardware Pages
### Stable control identity for UI, hardware, and future expansion

This document defines **how ControlIds are assigned, grouped, and evolved**.
ControlIds are a **public contract** between UI, DSP, hardware input, and hardware output.

Once released, they must be treated as **stable API**.

---

## 1. What is a ControlId?

A `ControlId` uniquely identifies a *user-facing control*.

It is used by:
- UI focus system
- BindingRegistry
- Hardware input routing
- Hardware output feedback
- Focus persistence

It is **not**:
- A parameter index
- A UI component pointer
- A JUCE-specific concept

Think of it as a **semantic address**.

---

## 2. ControlId rules (non-negotiable)

- ControlIds are **integers**
- ControlIds must be **stable once released**
- ControlIds must be **unique**
- ControlIds must never be auto-generated
- ControlIds must not be reused for different meanings

> Changing a ControlId breaks hardware mappings and session recall.

---

## 3. Recommended ID ranges

Use **numeric ranges** to future-proof hardware pages and grouping.

### Example layout

```
1000–1099  Core gain & level
1100–1199  Dynamics
1200–1299  EQ
1300–1399  Modulation
1400–1499  Time / FX
2000–2099  Global / utility
```

This allows hardware to map:
- Pages
- Encoder banks
- Screens

Without guessing.

---

## 4. Example ControlId definitions

```cpp
// Core
constexpr ui_core::ControlId kGainControlId    = 1001;
constexpr ui_core::ControlId kOutputControlId  = 1002;

// Dynamics
constexpr ui_core::ControlId kThresholdId      = 1101;
constexpr ui_core::ControlId kRatioId          = 1102;
```

Keep these in a **single header** if the plugin grows.

---

## 5. Hardware pages (future concept)

Hardware pages allow a limited set of physical controls to access many parameters.

### Conceptual mapping

```
Page 0 → 1000–1007 (Gain / Output)
Page 1 → 1100–1107 (Dynamics)
Page 2 → 1200–1207 (EQ)
```

The template does not enforce pages yet — but ControlIds are designed to support them cleanly.

---

## 6. Focus + pages interaction

Focus always refers to a **ControlId**, not a page.

When pages are added:
- Page selects a *group*
- Focus selects an *element*

This avoids:
- Focus ambiguity
- Hardware desync
- UI confusion

---

## 7. Adding new controls safely

When adding a new control:

1. Choose the correct numeric range
2. Assign a new ControlId
3. Never renumber existing IDs
4. Never overload meaning

If unsure — **append, don’t shuffle**.

---

## 8. Migration rules (advanced)

If you must change a ControlId:
- Treat it as a breaking change
- Provide migration logic
- Version plugin state

Default rule:
> Don’t change ControlIds.

---

## 9. Mental model

ControlIds are:
- More stable than UI layout
- More stable than parameter order
- As important as parameter names

Design them early. Protect them always.
