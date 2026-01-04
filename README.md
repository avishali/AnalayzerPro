# JUCE Plugin Template  
### Hardware-Ready, Focus-Aware, Scalable Architecture

This repository is a **production-ready JUCE plugin template** designed for building modern audio plugins that scale cleanly from simple UI-only tools to **hardware-integrated control-surface workflows**.

It is intentionally opinionated and optimized for:
- Long-term maintainability
- Explicit ownership of state
- Hardware-first interaction models
- Focus-aware UI and control routing

This is **not** a demo project or a JUCE tutorial — it is a foundation meant to be copied and evolved.

---

## 1. What this template is (and is not)

### ✅ What it *is*
- A clean JUCE + CMake plugin base
- Explicit DSP / UI / platform separation
- Normalized control routing (0..1)
- Focus-aware UI
- Hardware input & output abstraction
- Host-persistent UI state (window size, focus)
- Suitable for AU / VST3 / Standalone

### ❌ What it *is not*
- Not APVTS-based
- Not tied to MIDI / HID / OSC specifically
- Not a JUCE “example” project
- Not designed for quick hacks or experiments

---

## 2. High-level architecture

```
DSP (Parameters)
   ↑
BindingRegistry (normalized 0..1)
   ↑                ↑
Hardware Input      Hardware Output
   ↑                ↑
Controllers        LEDs / Focus / Displays
```

**Core rules:**
- Parameters own truth
- Hardware always works in normalized space
- UI uses native units
- All routing goes through bindings

---

## 3. Project structure

```
Source/
├── parameters/
│   ├── Parameters.h / .cpp
│   └── DSP state + persistence (single source of truth)
│
├── ui/
│   ├── MainView.h / .cpp
│   └── UI, focus, bindings, layout
│
├── hardware/
│   ├── PluginHardwareAdapter.h / .cpp        (input)
│   ├── PluginHardwareOutputAdapter.h / .cpp  (output)
│
ui_core/
├── FocusManager
├── BindingRegistry
├── Hardware contracts
└── JUCE-free platform layer
```

---

## 4. Parameters layer (DSP authority)

### Responsibilities
- Own parameter values
- Clamp and validate
- Persist plugin + UI state
- No UI or hardware knowledge

### Example pattern

```cpp
void Parameters::setGain (float v) noexcept
{
    gain.store (juce::jlimit (0.0f, 2.0f, v));
}
```

**Rule:**  
> Never clamp in the UI. Never clamp in hardware adapters.

---

## 5. BindingRegistry (the routing spine)

### Purpose
`BindingRegistry` maps **ControlId → parameter behavior** in normalized space.

Each binding defines:
- How to read a parameter (native → normalized)
- How to write a parameter (normalized → native)

### Why normalized?
- Hardware encoders don’t care about dB / Hz / gain
- Relative encoder math becomes trivial
- LED rings and displays become consistent
- Parameter ranges can change without touching hardware logic

---

## 6. Focus system

### Concepts
- Every interactive control has a `ControlId`
- `FocusManager` tracks the active control
- Focus drives:
  - UI highlight
  - Hardware focus LEDs
  - Which control receives encoder input

### Persistence
The focused control is saved in plugin state and restored on open.

---

## 7. Hardware input

### Input path

```
HardwareControlEvent
   → PluginHardwareAdapter
   → BindingRegistry
   → Parameters
```

Supports:
- Absolute input (faders, touch)
- Relative input (encoders)

Hardware adapters:
- Do **not** know parameter ranges
- Do **not** talk to DSP directly
- Only operate in normalized space

---

## 8. Hardware output

### Output path

```
Parameter change / Focus change
   → PluginHardwareOutputAdapter
   → LEDs / rings / displays
```

Current implementation:
- DBG-based (proof of behavior)

Future implementations:
- MIDI CC feedback
- HID controllers
- OSC / network control
- Custom hardware devices

---

## 9. UI resizing & persistence

### Resizing
Editor resizing is **opt-in**, controlled via CMake:

```bash
-DPLUGIN_EDITOR_RESIZABLE=ON
```

Default builds are **not resizable**.

### Persistence
- Window size is stored in `Parameters`
- Host persists it with plugin state
- Works across sessions and reloads

---

## 10. How to add a new parameter (checklist)

1. **Add to `Parameters`**
   - Atomic value
   - Getter / setter
   - Persistence

2. **Assign a new ControlId**
   ```cpp
   constexpr ui_core::ControlId kMyParamId = 1003;
   ```

3. **Add binding**
   - Normalized mapping
   - UI update
   - Hardware output

4. **Register focus**
   - FocusManager
   - Optional focus outline

That’s it. No other layers need changes.

---

## 11. How to add real hardware support

Replace:
- `PluginHardwareAdapter`
- `PluginHardwareOutputAdapter`

With implementations for:
- MIDI
- HID
- OSC
- Network protocols

No changes required in:
- UI
- Parameters
- Bindings
- Focus system

---

## 12. Template philosophy

This template optimizes for:
- Explicit ownership
- Predictable behavior
- Hardware-first workflows
- Long-term scalability

It intentionally avoids:
- Implicit magic
- Over-abstracted frameworks
- “JUCE demo” patterns

---

## 13. Recommended workflow

1. Clone or copy this repository
2. Rename plugin identifiers
3. Start adding DSP
4. Reuse platform layers unchanged
5. Extend hardware I/O as needed

- 📐 **Architecture Overview**  
  [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md)  
  How control, focus, hardware input/output, and DSP state flow through the system.

- 🧠 **Template Philosophy**  
  [`TEMPLATE_PHILOSOPHY.md`](TEMPLATE_PHILOSOPHY.md)  
  The design principles and non-negotiable rules behind the architecture.

- ✅ **New Plugin Checklist**  
  [`NEW_PLUGIN_CHECKLIST.md`](NEW_PLUGIN_CHECKLIST.md)  
  Step-by-step process for creating a new plugin safely from this template.



---

## Status

This template currently supports:
- Multi-parameter focus
- Hardware input & output abstraction
- Focus persistence
- Window size persistence
- Standalone / AU / VST3

It is considered **stable** and suitable for reuse across projects.

---

## Documentation

- 🎛 **Control IDs & Hardware Pages**  
  [`docs/CONTROL_IDS.md`](docs/CONTROL_IDS.md)  
  How ControlIds are assigned, kept stable, and scaled for future hardware pages.


For deeper understanding of the template architecture and rules, see:

### Docs map

README.md
├─ docs/ARCHITECTURE.md
│ (how the system works)
│
├─ TEMPLATE_PHILOSOPHY.md
│ (why the rules exist)
│
└─ NEW_PLUGIN_CHECKLIST.md
(how to start a new plugin safely)


