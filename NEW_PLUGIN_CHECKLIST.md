# NEW_PLUGIN_CHECKLIST.md
### Step-by-step guide for creating a new plugin from the JUCE Plugin Template

This checklist is designed to be followed **top to bottom** whenever you start a new plugin using this template.
If you follow these steps in order, you will avoid architectural drift and keep all plugins consistent.

> 🔗 Related documents:  
> - [README](README.md)  
> - [Architecture Overview](docs/ARCHITECTURE.md)  
> - [Template Philosophy](TEMPLATE_PHILOSOPHY.md)


---

## 1. Repository & Identity

- [ ] Copy or fork the template repository
- [ ] Rename the repository to the new plugin name
- [ ] Update `CMakeLists.txt`:
  - [ ] `PROJECT_NAME`
  - [ ] `COMPANY_NAME`
  - [ ] Plugin formats (AU / VST3 / Standalone)
- [ ] Update bundle identifiers (reverse-DNS)
- [ ] Rename binary / artefact names if required

---

## 2. Build Sanity Check (before changes)

- [ ] Configure CMake with correct JUCE path
  ```bash
  cmake -S . -B build -DJUCE_PATH=/path/to/JUCE
  ```
- [ ] Build the project
- [ ] Run Standalone
- [ ] Confirm plugin opens with no errors

> ⚠️ Do not start editing DSP or UI until this passes.

---

## 3. Parameters (DSP Authority)

For **each new parameter**:

- [ ] Add atomic value to `Parameters`
- [ ] Add getter
- [ ] Add setter with clamping
- [ ] Add persistence to state (`getState` / `setState`)
- [ ] Choose native range carefully (not normalized)

Rule:
> Parameters own truth. Never clamp elsewhere.

---

## 4. Control IDs

- [ ] Assign a unique `ControlId` for each parameter
- [ ] Keep IDs stable once released
- [ ] Group IDs logically (future hardware pages)

Example:
```cpp
constexpr ui_core::ControlId kGainControlId   = 1001;
constexpr ui_core::ControlId kOutputControlId = 1002;
```

---

## 5. UI Controls

For each parameter:

- [ ] Create slider / control
- [ ] Set native range (matches Parameters)
- [ ] Add label
- [ ] Add to layout in `MainView::resized()`
- [ ] Ensure controls do not overlap at minimum window size

---

## 6. Bindings (normalized routing)

For each parameter:

- [ ] Add a mapped binding to `BindingRegistry`
- [ ] Define:
  - [ ] `toNative(normalized)`
  - [ ] `toNormalized(native)`
- [ ] Update UI inside setter (use `dontSendNotification`)
- [ ] Send hardware LED feedback (normalized)

Rule:
> All routing goes through bindings.

---

## 7. Focus System

For each interactive control:

- [ ] Register with `FocusManager`
- [ ] Set focus on `onDragStart`
- [ ] Draw focus outline in `paint()`
- [ ] Persist focus to `Parameters`
- [ ] Restore focus on editor open

Verify:
- [ ] Tab cycles focus correctly
- [ ] Hardware focus output updates

---

## 8. Hardware Input

- [ ] Route all simulated / real hardware input through `PluginHardwareAdapter`
- [ ] Use normalized values (0..1)
- [ ] Use relative mode for encoders
- [ ] Never write to Parameters directly

Test:
- [ ] Absolute events work
- [ ] Relative events accumulate correctly

---

## 9. Hardware Output

- [ ] Implement `PluginHardwareOutputAdapter`
- [ ] Send focus changes
- [ ] Send normalized value updates
- [ ] Verify DBG output (or real hardware)

Rule:
> Hardware output mirrors state — it never drives it.

---

## 10. UI Resizing (optional)

If the plugin should be resizable:

- [ ] Enable via CMake:
  ```bash
  -DPLUGIN_EDITOR_RESIZABLE=ON
  ```
- [ ] Verify min/max size limits
- [ ] Verify layout scales correctly
- [ ] Verify window size persistence

If not needed:
- [ ] Leave resizing disabled (default)

---

## 11. Persistence Checklist

Confirm the following survive reopen / reload:

- [ ] Parameter values
- [ ] Focused control
- [ ] Window size (if enabled)

Test in:
- [ ] Standalone
- [ ] At least one DAW

---

## 12. Cleanup Before First Commit

- [ ] Remove unused demo code
- [ ] Remove unused ControlIds
- [ ] Remove DBG spam
- [ ] Commit with a clean message:
  ```
  Initial plugin scaffold based on template
  ```

---

## 13. Template Rules (do not break)

- ❌ No direct DSP writes from UI
- ❌ No hardware → DSP shortcuts
- ❌ No UI clamping
- ❌ No parameter duplication
- ✅ One source of truth
- ✅ Explicit routing
- ✅ Normalized hardware

---

## 14. Ready for DSP Work

Once all above is checked:

- ✅ Architecture is stable
- ✅ Hardware-ready
- ✅ Safe to add DSP, modulation, automation

You are now building **on top of the platform**, not fighting it.
