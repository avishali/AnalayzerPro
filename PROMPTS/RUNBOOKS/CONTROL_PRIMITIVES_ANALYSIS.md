# CONTROL PRIMITIVES EXTRACTION — Pattern Analysis
**Reference: AnalyzerPro ControlRail**

## STEP 1: IDENTIFIED PRIMITIVES

### 1. SectionHeader Pattern
**Instances**: `navigateLabel`, `analyzerLabel`, `displayLabel`, `metersLabel`

**Pattern**:
```cpp
label.setText ("Section Name", juce::dontSendNotification);
label.setFont (type.sectionTitleFont());
label.setJustificationType (juce::Justification::centredLeft);
label.setColour (juce::Label::textColourId, theme.lightGrey);
addAndMakeVisible (label);
```

**Layout**:
```cpp
label.setBounds (bounds.getX(), y, bounds.getWidth(), m.titleHeight);
y += m.titleHeight + m.titleSecondaryGap;
```

**Repeated**: 4 times (identical pattern)

---

### 2. ChoiceRow Pattern (Label + Combo)
**Instances**: 
- `modeLabel` + `modeCombo`
- `fftSizeLabel` + `fftSizeCombo`
- `averagingLabel` + `averagingCombo`
- `dbRangeLabel` + `dbRangeCombo`
- `tiltLabel` + `tiltCombo`

**Pattern**:
```cpp
// Label setup
label.setText ("Label Text", juce::dontSendNotification);
label.setFont (type.labelSmallFont());
label.setJustificationType (juce::Justification::centredLeft);
label.setColour (juce::Label::textColourId, theme.grey);
addAndMakeVisible (label);

// Combo setup
combo.addItem (...);
combo.setSelectedId (...);
addAndMakeVisible (combo);
```

**Layout**:
```cpp
label.setBounds (bounds.getX(), y, bounds.getWidth(), m.secondaryHeight);
y += m.secondaryHeight;
combo.setBounds (bounds.getX(), y, bounds.getWidth(), m.comboH);
y += m.comboH + m.gapSmall;
```

**Repeated**: 5 times (identical pattern)

---

### 3. ToggleRow Pattern (Label + Toggle)
**Instances**:
- `peakHoldLabel` + `peakHoldButton`
- `holdLabel` + `holdButton`

**Pattern**:
```cpp
// Label setup (same as ChoiceRow)
label.setText ("Label Text", juce::dontSendNotification);
label.setFont (type.labelSmallFont());
label.setJustificationType (juce::Justification::centredLeft);
label.setColour (juce::Label::textColourId, theme.grey);
addAndMakeVisible (label);

// Toggle setup
toggle.setButtonText ("Button Text");
toggle.setToggleState (defaultState, juce::dontSendNotification);
addAndMakeVisible (toggle);
```

**Layout**:
```cpp
label.setBounds (bounds.getX(), y, bounds.getWidth(), m.secondaryHeight);
y += m.secondaryHeight;
toggle.setBounds (bounds.getX(), y, m.buttonSmallW, m.buttonSmallH);
y += m.buttonSmallH + m.gapSmall;
```

**Repeated**: 2 times (identical pattern)

---

### 4. SliderRow Pattern (Label + Slider)
**Instances**:
- `peakDecayLabel` + `peakDecaySlider`
- `displayGainLabel` + `displayGainSlider`

**Pattern**:
```cpp
// Label setup (same as ChoiceRow)
label.setText ("Label Text", juce::dontSendNotification);
label.setFont (type.labelSmallFont());
label.setJustificationType (juce::Justification::centredLeft);
label.setColour (juce::Label::textColourId, theme.grey);
addAndMakeVisible (label);

// Slider setup
slider.setSliderStyle (juce::Slider::LinearHorizontal);
slider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 35, static_cast<int> (type.labelSmallH + 5));
slider.setRange (...);
slider.setValue (...);
addAndMakeVisible (slider);
```

**Layout**:
```cpp
label.setBounds (bounds.getX(), y, bounds.getWidth(), m.secondaryHeight);
y += m.secondaryHeight;
slider.setBounds (bounds.getX(), y, bounds.getWidth(), m.sliderH);
y += m.sliderH + m.gapSmall;
```

**Repeated**: 2 times (identical pattern)

**Magic Numbers Found**:
- `35` - text box width (should be a metric)
- `type.labelSmallH + 5` - text box height (the `+ 5` should be a metric)

---

### 5. ControlGroup Pattern (Vertical Stack)
**Pattern**: Sequential layout of controls within a section
```cpp
// Section header
sectionLabel.setBounds (bounds.getX(), y, bounds.getWidth(), m.titleHeight);
y += m.titleHeight + m.titleSecondaryGap;

// Control rows (ChoiceRow, ToggleRow, SliderRow)
// ... repeated pattern

// Section spacing
y += m.sectionSpacing;  // or m.gapSmall for last item before next section
```

**Repeated**: Used for all sections (Navigate, Analyzer, Display, Meters)

---

## STEP 2: MAGIC NUMBERS IDENTIFIED

### Hardcoded Values (Need Normalization):

1. **Line 145, 157**: `35` - Slider text box width
   ```cpp
   slider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 35, ...);
   ```
   **Should be**: `m.sliderTextBoxW` (add to Metrics)

2. **Line 145, 157**: `+ 5` - Slider text box height offset
   ```cpp
   static_cast<int> (type.labelSmallH + 5)
   ```
   **Should be**: `m.sliderTextBoxH` (add to Metrics, or use existing metric)

3. **Line 277**: `50` - resetPeaksButton width
   ```cpp
   resetPeaksButton.setBounds (..., 50, m.buttonSmallH);
   ```
   **Should be**: `m.buttonW` or `m.buttonSmallW` (use existing metric)

---

## STEP 2: NORMALIZATION PLAN

### Actions Required:

1. **Add missing metrics to `mdsp_ui::Metrics`**:
   - `sliderTextBoxW = 35` - Slider text box width
   - `sliderTextBoxH` - Slider text box height (or derive from existing)

2. **Update ControlRail.cpp**:
   - Replace `35` with `m.sliderTextBoxW`
   - Replace `type.labelSmallH + 5` with `m.sliderTextBoxH`
   - Replace `50` with `m.buttonW` (or appropriate existing metric)

3. **Verify all usage**:
   - All colors from `theme`
   - All fonts from `type`
   - All spacing from `m`
   - No magic numbers remaining

---

## STEP 3: EXTRACTION PLAN (Optional, Safe Mode)

### Local Helper Classes (Inside Plugin Only):

If extracting primitives, create in `Source/ui/layout/control_primitives/`:

1. **`SectionHeader.h/cpp`**:
   - Encapsulates section title setup and layout
   - Constructor: `SectionHeader (UiContext&, const juce::String&)`
   - Method: `void layout (juce::Rectangle<int>& area, int& y)`

2. **`ChoiceRow.h/cpp`**:
   - Encapsulates Label + Combo setup and layout
   - Constructor: `ChoiceRow (UiContext&, const juce::String&, juce::ComboBox&)`
   - Method: `void layout (juce::Rectangle<int>& area, int& y)`

3. **`ToggleRow.h/cpp`**:
   - Encapsulates Label + Toggle setup and layout
   - Constructor: `ToggleRow (UiContext&, const juce::String&, juce::ToggleButton&)`
   - Method: `void layout (juce::Rectangle<int>& area, int& y)`

4. **`SliderRow.h/cpp`**:
   - Encapsulates Label + Slider setup and layout
   - Constructor: `SliderRow (UiContext&, const juce::String&, juce::Slider&, float min, float max, float step)`
   - Method: `void layout (juce::Rectangle<int>& area, int& y)`

**Rules**:
- Keep all classes private/implementation detail
- No public API exposure
- Keep ControlRail compiling at all times
- Extract incrementally (one primitive at a time)

---

## CURRENT STATUS

- ✅ STEP 0: Session initialization complete
- 🔄 STEP 1: Pattern identification complete (this document)
- ⏳ STEP 2: Normalization pending
- ⏳ STEP 3: Extraction pending (optional)
