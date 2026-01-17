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

- [x] Copy or fork the template repository
- [x] Rename the repository to the new plugin name
- [x] Update `CMakeLists.txt`:
  - [x] `PROJECT_NAME`
  - [x] `COMPANY_NAME`
  - [x] Plugin formats (AU / VST3 / Standalone)
- [x] Update bundle identifiers (reverse-DNS)
- [x] Rename binary / artefact names if required

---

## 2. Build Sanity Check (before changes)

- [x] Configure CMake with correct JUCE path
  ```bash
  cmake -S . -B build -DJUCE_PATH=/path/to/JUCE
  ```
- [x] Build the project
- [x] Run Standalone
- [x] Confirm plugin opens with no errors

> ⚠️ Do not start editing DSP or UI until this passes.

---

## 3. Parameters (DSP Authority)

For **each new parameter**:

- [x] Add atomic value to `Parameters`
- [x] Add getter
- [x] Add setter with clamping
- [x] Add persistence to state (`getState` / `setState`)
- [x] Choose native range carefully (not normalized)

Rule:
> Parameters own truth. Never clamp elsewhere.

---

## 4. Control IDs

- [x] Assign a unique `ControlId` for each parameter
- [x] Keep IDs stable once released
- [x] Group IDs logically (future hardware pages)

Example:
```cpp
constexpr ui_core::ControlId kGainControlId   = 1001;
constexpr ui_core::ControlId kOutputControlId = 1002;
```

---

## 5. UI Controls

For each parameter:

- [x] Create slider / control
- [x] Set native range (matches Parameters)
- [x] Add label
- [x] Add to layout in `MainView::resized()`
- [x] Ensure controls do not overlap at minimum window size

---

## 6. Bindings (normalized routing)

For each parameter:

- [x] Add a mapped binding to `BindingRegistry`
- [x] Define:
  - [x] `toNative(normalized)`
  - [x] `toNormalized(native)`
- [x] Update UI inside setter (use `dontSendNotification`)
- [x] Send hardware LED feedback (normalized)

Rule:
> All routing goes through bindings.

---

## 7. Focus System

For each interactive control:

- [x] Register with `FocusManager`
- [x] Set focus on `onDragStart`
- [x] Draw focus outline in `paint()`
- [x] Persist focus to `Parameters`
- [x] Restore focus on editor open

Verify:
- [x] Tab cycles focus correctly
- [x] Hardware focus output updates

---

## 8. Hardware Input

- [x] Route all simulated / real hardware input through `PluginHardwareAdapter`
- [x] Use normalized values (0..1)
- [x] Use relative mode for encoders
- [x] Never write to Parameters directly

Test:
- [ ] Absolute events work
- [ ] Relative events accumulate correctly

---

## 9. Hardware Output

- [x] Implement `PluginHardwareOutputAdapter`
- [x] Send focus changes
- [x] Send normalized value updates
- [x] Verify DBG output (or real hardware)

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

- [x] Parameter values
- [x] Focused control
- [x] Window size (if enabled)

Test in:
- [x] Standalone
- [x] At least one DAW

---

## 12. Cleanup Before First Commit

- [x] Remove unused demo code
- [x] Remove unused ControlIds
- [x] Remove DBG spam
- [x] Commit with a clean message:
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
