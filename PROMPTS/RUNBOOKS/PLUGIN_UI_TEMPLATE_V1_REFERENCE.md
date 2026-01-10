# PLUGIN UI TEMPLATE v1 — Reference Implementation Guide
**Based on AnalyzerPro (after MELECHDSP UI SYSTEM v1 completion)**

## Overview

This document describes the AnalyzerPro UI structure as the **reference implementation** for creating new MelechDSP plugins using the MelechDSP UI Design System v1.

**Status**: ✅ AnalyzerPro has completed Steps 1-3 of MELECHDSP UI SYSTEM v1, making it the canonical reference.

---

## Reference Implementation: AnalyzerPro UI Structure

### PluginEditor (UiContext Owner)
**Location**: `Source/PluginEditor.{h,cpp}`

**Structure**:
```cpp
class AnalayzerProAudioProcessorEditor : public juce::AudioProcessorEditor
{
    AnalayzerProAudioProcessor& audioProcessor;
    mdsp_ui::UiContext ui_;  // ✅ Single instance (owns Theme, Metrics, Typography)
    MainView mainView;       // ✅ Receives ui_ by reference
};
```

**Key Points**:
- `PluginEditor` owns `mdsp_ui::UiContext` instance
- `UiContext` is initialized with `ThemeVariant::Dark` (Phase 1)
- `MainView` constructor receives `ui_` by reference: `mainView (ui_, p, &p.getAPVTS())`
- No duplication — single `UiContext` shared with all children

---

### MainView (Layout Container)
**Location**: `Source/ui/MainView.{h,cpp}`

**Structure**:
```cpp
class MainView : public juce::Component,
                 public juce::AudioProcessorValueTreeState::Listener
{
    explicit MainView (mdsp_ui::UiContext& ui, 
                       AnalayzerProAudioProcessor& p, 
                       juce::AudioProcessorValueTreeState* apvts);
    
    mdsp_ui::UiContext& ui_;  // ✅ Reference (not owned)
    
    // Layout components (all receive ui_ reference)
    HeaderBar header_;
    FooterBar footer_;
    ControlRail rail_;
    AnalyzerDisplayView analyzerView_;  // (doesn't use UiContext yet)
    MeterGroupComponent outputMeters_;
    MeterGroupComponent inputMeters_;
};
```

**Layout Structure** (from `resized()`):
```
┌─────────────────────────────────────────┐
│ HeaderBar (top)                         │
├──────┬─────────────────────────┬────────┤
│      │                         │        │
│ Input│    AnalyzerDisplayView  │ Output │
│Meters│    (center, main scope) │ Meters │
│      │                         │        │
│      │                         │        │
│      ├─────────────────────────┤        │
│      │ ControlRail (bottom)    │        │
├──────┴─────────────────────────┴────────┤
│ FooterBar (bottom)                      │
└─────────────────────────────────────────┘
```

**Key Points**:
- `MainView` receives `UiContext&` by reference (does not own)
- All child components receive `ui_` reference in constructor
- Layout uses `ui_.metrics()` for spacing/padding (no hardcoded values)
- Resizable layout (no `setSize()` inside views)

---

### HeaderBar Component
**Location**: `Source/ui/layout/HeaderBar.{h,cpp}`

**Structure**:
```cpp
class HeaderBar : public juce::Component
{
    explicit HeaderBar (mdsp_ui::UiContext& ui);
    
    mdsp_ui::UiContext& ui_;
    
    // Components use ui_.theme() and ui_.type()
    juce::Label titleLabel;
    juce::Label modeLabel;  // Read-only (mirrors right-side control)
    // ... other controls
};
```

**Usage Pattern**:
```cpp
HeaderBar::HeaderBar (mdsp_ui::UiContext& ui) : ui_ (ui)
{
    const auto& theme = ui_.theme();
    const auto& type = ui_.type();
    
    titleLabel.setFont (type.titleFont());
    titleLabel.setColour (juce::Label::textColourId, theme.lightGrey);
}

void HeaderBar::paint (juce::Graphics& g)
{
    const auto& theme = ui_.theme();
    g.fillAll (theme.black);
    g.setColour (theme.borderDivider);
    // ...
}

void HeaderBar::resized()
{
    const auto& m = ui_.metrics();
    auto area = getLocalBounds().reduced (m.pad);
    // Use m.pad, m.gap, m.headerButtonW, etc.
}
```

**Key Points**:
- Constructor accepts `UiContext&` reference
- Stores as reference member: `mdsp_ui::UiContext& ui_;`
- Uses `ui_.theme()`, `ui_.metrics()`, `ui_.type()` in paint/resized
- No hardcoded colors, fonts, or spacing values

---

### FooterBar Component
**Location**: `Source/ui/layout/FooterBar.{h,cpp}`

**Structure**:
```cpp
class FooterBar : public juce::Component
{
    explicit FooterBar (mdsp_ui::UiContext& ui);
    
    mdsp_ui::UiContext& ui_;
    juce::Label statusLabel;
};
```

**Usage Pattern**: Same as HeaderBar — uses `ui_.theme()`, `ui_.metrics()`, `ui_.type()`

---

### ControlRail Component
**Location**: `Source/ui/layout/ControlRail.{h,cpp}`

**Structure**:
```cpp
class ControlRail : public juce::Component
{
    explicit ControlRail (mdsp_ui::UiContext& ui);
    
    mdsp_ui::UiContext& ui_;
    
    // Section titles use type.sectionTitleFont()
    // Labels use type.labelSmallFont()
    // Combos use m.comboH, sliders use m.sliderH
    // Colors use theme.grey, theme.lightGrey
};
```

**Usage Pattern**: Same pattern — all metrics/colors/fonts from `ui_`

---

### MeterGroupComponent / MeterComponent
**Location**: `Source/ui/meters/MeterGroupComponent.{h,cpp}`, `MeterComponent.{h,cpp}`

**Structure**:
```cpp
class MeterGroupComponent : public juce::Component
{
    MeterGroupComponent (mdsp_ui::UiContext& ui,
                         AnalayzerProAudioProcessor& processor, GroupType type);
    
    mdsp_ui::UiContext& ui_;
    
    // Creates MeterComponent instances with ui_ reference
    std::unique_ptr<MeterComponent> meter0_;
    std::unique_ptr<MeterComponent> meter1_;
};

class MeterComponent : public juce::Component
{
    MeterComponent (mdsp_ui::UiContext& ui,
                    const std::atomic<float>* peakDb,
                    const std::atomic<float>* rmsDb,
                    const std::atomic<bool>* clipLatched,
                    juce::String labelText);
    
    mdsp_ui::UiContext& ui_;
};
```

**Key Points**:
- `MeterGroupComponent` receives `UiContext&` and passes it to `MeterComponent` instances
- All colors/metrics/fonts sourced from `ui_`

---

## Template Pattern Summary

### For New Plugins (Following PLUGIN_UI_TEMPLATE_V1):

#### STEP 1: MainView Scaffold
```cpp
// PluginEditor.h
class NewPluginEditor : public juce::AudioProcessorEditor
{
    mdsp_ui::UiContext ui_;  // ✅ Owner
    MainView mainView;
};

// MainView.h
class MainView : public juce::Component
{
    explicit MainView (mdsp_ui::UiContext& ui, ...);
    mdsp_ui::UiContext& ui_;  // ✅ Reference
    
    HeaderBar header_;
    FooterBar footer_;
    ControlRail rail_;
    // ... other components
};

// MainView.cpp
MainView::MainView (mdsp_ui::UiContext& ui, ...)
    : ui_ (ui),
      header_ (ui_),    // ✅ Pass reference
      footer_ (ui_),    // ✅ Pass reference
      rail_ (ui_)       // ✅ Pass reference
{
}
```

#### STEP 2: UI Context Injection
```cpp
// PluginEditor.cpp
NewPluginEditor::NewPluginEditor (NewPluginProcessor& p)
    : AudioProcessorEditor (&p),
      ui_ (mdsp_ui::ThemeVariant::Dark),  // ✅ Default to Dark (Phase 1)
      mainView (ui_, p, &p.getAPVTS())    // ✅ Pass reference
{
}
```

#### STEP 3: Placeholder Visuals
- Use `ui_.theme()` for all colors
- Use `ui_.metrics()` for all spacing/layout
- Use `ui_.type()` for all fonts
- No hardcoded values

---

## Files to Copy/Reference for New Plugins

### Must Copy Structure:
1. **`Source/PluginEditor.{h,cpp}`** - UiContext owner pattern
2. **`Source/ui/MainView.{h,cpp}`** - Layout container pattern
3. **`Source/ui/layout/HeaderBar.{h,cpp}`** - Header pattern
4. **`Source/ui/layout/FooterBar.{h,cpp}`** - Footer pattern
5. **`Source/ui/layout/ControlRail.{h,cpp}`** - Control panel pattern

### Reference Only (Customize per Plugin):
- `Source/ui/meters/` - Meter components (only if needed)
- `Source/ui/analyzer/` - AnalyzerDisplayView (customize for plugin's main view)
- `Source/ui/views/` - Placeholder views

---

## Key Patterns from AnalyzerPro

### Pattern 1: UiContext Injection
```cpp
// ✅ Correct: Component receives UiContext&
Component::Component (mdsp_ui::UiContext& ui) : ui_ (ui) {}

// ❌ Wrong: Component creates local Theme
void Component::paint (juce::Graphics& g)
{
    mdsp_ui::Theme theme;  // Don't do this
}
```

### Pattern 2: Theme/Metrics/Typography Usage
```cpp
// ✅ Correct: Use ui_ accessors
void Component::paint (juce::Graphics& g)
{
    const auto& theme = ui_.theme();
    const auto& m = ui_.metrics();
    const auto& type = ui_.type();
    
    g.fillAll (theme.background);
    g.setFont (type.labelFont());
    // Use m.pad, m.gap, etc.
}

// ❌ Wrong: Hardcoded values
void Component::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black);  // Don't do this
    g.setFont (juce::Font (11.0f));    // Don't do this
}
```

### Pattern 3: Layout Metrics
```cpp
// ✅ Correct: Use ui_.metrics()
void Component::resized()
{
    const auto& m = ui_.metrics();
    auto area = getLocalBounds().reduced (m.pad);
    combo.setBounds (area.removeFromTop (m.comboH));
    area.removeFromTop (m.gap);
    slider.setBounds (area.removeFromTop (m.sliderH));
}

// ❌ Wrong: Hardcoded spacing
void Component::resized()
{
    auto area = getLocalBounds().reduced (10);  // Don't do this
    combo.setBounds (area.removeFromTop (20));  // Don't do this
}
```

---

## Checklist for New Plugin UI

Based on AnalyzerPro reference:

### ✅ STEP 1: MainView Scaffold
- [ ] `MainView` owns layout only (no rendering logic)
- [ ] Layout regions match AnalyzerPro structure:
  - [ ] HeaderBar (top)
  - [ ] FooterBar (bottom)
  - [ ] ControlRail (bottom center, optional)
  - [ ] Main visualization (center)
  - [ ] Optional: Meter rails (left/right)
- [ ] `resized()` uses `ui_.metrics()` for all spacing
- [ ] No `setSize()` inside views
- [ ] Layout is resizable

### ✅ STEP 2: UI Context Injection
- [ ] `PluginEditor` owns `mdsp_ui::UiContext` instance
- [ ] `UiContext` initialized with `ThemeVariant::Dark` (Phase 1)
- [ ] `MainView` receives `UiContext&` by reference
- [ ] All child components receive same `UiContext&` reference
- [ ] No duplication — single instance shared

### ✅ STEP 3: Placeholder Visuals
- [ ] All components use `ui_.theme()` for colors
- [ ] All components use `ui_.metrics()` for spacing
- [ ] All components use `ui_.type()` for fonts
- [ ] No hardcoded `juce::Colours::*` (except through theme)
- [ ] No hardcoded font sizes
- [ ] No hardcoded padding/gaps

---

## Differences from AnalyzerPro (Customize Per Plugin)

### What Changes Per Plugin:
- **Main visualization component** (AnalyzerDisplayView → YourPluginView)
- **ControlRail controls** (Mode/FFT Size/Averaging → Your plugin's parameters)
- **HeaderBar content** (title, mode selector → Your plugin's header)
- **FooterBar status** (status text → Your plugin's status)

### What Stays the Same:
- **UiContext injection pattern** (PluginEditor → MainView → children)
- **Layout structure** (Header/Footer/Center/Controls)
- **Theme/Metrics/Typography usage** (all from `ui_`)
- **No hardcoded values** (all from design system)

---

## Reference Files Checklist

When creating a new plugin, reference these AnalyzerPro files:

- ✅ `Source/PluginEditor.{h,cpp}` - UiContext owner pattern
- ✅ `Source/ui/MainView.{h,cpp}` - Layout container with UiContext injection
- ✅ `Source/ui/layout/HeaderBar.{h,cpp}` - Header with UiContext usage
- ✅ `Source/ui/layout/FooterBar.{h,cpp}` - Footer with UiContext usage
- ✅ `Source/ui/layout/ControlRail.{h,cpp}` - Control panel with UiContext usage
- ✅ `Source/ui/meters/MeterGroupComponent.{h,cpp}` - Example of nested UiContext passing

---

## mdsp_ui Structures Reference

### Theme (from `mdsp_ui/Theme.h`)
```cpp
struct Theme
{
    // Backgrounds
    juce::Colour background;  // 0xff121212 (Dark)
    juce::Colour panel;       // 0xff1a1a1a (Dark)
    
    // Text
    juce::Colour text;        // 0xffe6e6e6 (Dark)
    juce::Colour textMuted;   // 0xffb0b0b0 (Dark)
    juce::Colour lightGrey;   // 0xffd3d3d3
    juce::Colour grey;        // 0xff808080
    juce::Colour darkGrey;    // 0xffa9a9a9
    
    // Accents
    juce::Colour accent;      // 0xff4fc3f7
    juce::Colour warning;     // 0xffffc107
    juce::Colour error;       // 0xffff5252
    // ... more colors
};
```

### Metrics (from `mdsp_ui/Metrics.h`)
```cpp
struct Metrics
{
    int pad = 10;           // Standard padding
    int padSmall = 4;       // Small padding
    int gap = 8;            // Standard gap
    int gapSmall = 2;       // Small gap
    int titleHeight = 14;   // Section title height
    int secondaryHeight = 12; // Secondary label height
    int comboH = 16;        // Combo box height
    int sliderH = 14;       // Slider height
    int buttonH = 22;       // Standard button height
    // ... more metrics
};
```

### Typography (from `mdsp_ui/Typography.h`)
```cpp
struct Typography
{
    float titleH = 14.0f;       // Main title
    float sectionTitleH = 12.0f; // Section titles
    float labelH = 11.0f;       // Standard labels
    float labelSmallH = 9.0f;   // Small labels
    
    juce::Font titleFont() const;
    juce::Font sectionTitleFont() const;
    juce::Font labelFont() const;
    juce::Font labelSmallFont() const;
    // ... more fonts
};
```

### UiContext (from `mdsp_ui/UiContext.h`)
```cpp
class UiContext
{
public:
    explicit UiContext (ThemeVariant variant = ThemeVariant::Dark);
    
    const Theme& theme() const noexcept;
    const Metrics& metrics() const noexcept;
    const Typography& type() const noexcept;
    
    ThemeVariant getVariant() const noexcept;
    void setVariant (ThemeVariant variant);  // Future: runtime switching
};
```

---

## Common Mistakes to Avoid

### ❌ Mistake 1: Creating Local Theme Instances
```cpp
// ❌ Wrong
void Component::paint (juce::Graphics& g)
{
    mdsp_ui::Theme theme;  // Creates default Dark theme
    g.fillAll (theme.background);
}
```

**Fix**: Store `UiContext&` and use `ui_.theme()`

### ❌ Mistake 2: Hardcoded Values
```cpp
// ❌ Wrong
void Component::resized()
{
    auto area = getLocalBounds().reduced (10);
    combo.setBounds (area.removeFromTop (20));
}
```

**Fix**: Use `ui_.metrics().pad` and `ui_.metrics().comboH`

### ❌ Mistake 3: Not Passing UiContext to Children
```cpp
// ❌ Wrong
MainView::MainView (mdsp_ui::UiContext& ui, ...)
    : ui_ (ui),
      header_ ()  // Missing ui_ parameter
{
}
```

**Fix**: Pass `ui_` to all child constructors: `header_ (ui_)`

### ❌ Mistake 4: Duplicating UiContext
```cpp
// ❌ Wrong
class PluginEditor
{
    mdsp_ui::UiContext ui1_;
    mdsp_ui::UiContext ui2_;  // Duplicate!
};
```

**Fix**: Single instance shared by reference

---

## Verification Checklist

After implementing a new plugin UI using this template:

- [ ] `PluginEditor` owns `UiContext` instance
- [ ] `MainView` receives `UiContext&` reference
- [ ] All child components receive `UiContext&` reference
- [ ] No components create local `Theme` instances
- [ ] No hardcoded `juce::Colours::*` (except through theme)
- [ ] No hardcoded font sizes (use `ui_.type()`)
- [ ] No hardcoded spacing (use `ui_.metrics()`)
- [ ] Layout is resizable (no `setSize()` inside views)
- [ ] All UI uses same `Theme/Metrics/Typography` instance
- [ ] Build passes with no warnings

---

**Reference Implementation**: AnalyzerPro (after MELECHDSP UI SYSTEM v1 Steps 1-3 completion)  
**Date**: 2024-12-19  
**Status**: ✅ Complete and validated
