`# Meter Module Status + AnalyzerPro Integration Plan

## Purpose

This document captures:
- what is already implemented in `melechdsp-hq/shared/mdsp_ui/meters`
- what AnalyzerPro still needs to do to use it end-to-end

## What Was Implemented In `mdsp_ui/meters`

Implemented in HQ under `shared/mdsp_ui` with strict layering.

### 1) Types + config

Added:
- `include/mdsp_ui/meters/MeterTypes.h`

Includes:
- enums: `MeterKind`, `MeterOrientation`, `MeterScale`, `MeterChannel`
- structs: `MeterBallisticsConfig`, `MeterConfig`, `MeterSnapshot`, `MeterRenderState`

### 2) Ballistics + hold logic

Added:
- `include/mdsp_ui/meters/MeterBallistics.h`
- `src/meters/MeterBallistics.cpp`
- `include/mdsp_ui/meters/PeakHoldModel.h`
- `src/meters/PeakHoldModel.cpp`

Behavior:
- attack/release smoothing
- peak hold timing + falloff
- clip hold timer support via config

### 3) Value model (thread-safe snapshot input)

Added:
- `include/mdsp_ui/meters/MeterValueModel.h`
- `src/meters/MeterValueModel.cpp`

Behavior:
- `pushSnapshot(const MeterSnapshot&)` uses atomic snapshot handoff
- `tick(dt)` computes smoothed render state
- `readRenderState()` returns copyable POD render state

### 4) Renderer (pure draw)

Added:
- `include/mdsp_ui/meters/MeterRenderer.h`
- `src/meters/MeterRenderer.cpp`

Behavior:
- stateless rendering from `MeterRenderState`
- no timers/mouse/APVTS
- uses theme tokens (`Theme`) for colors
- debug invariant added:
  - `jassert (juce::MessageManager::getInstance()->isThisTheMessageThread());`

### 5) Widgets (thin Components)

Added:
- `include/mdsp_ui/meters/widgets/LevelMeter.h`
- `include/mdsp_ui/meters/widgets/GainReductionMeter.h`
- `include/mdsp_ui/meters/widgets/StereoMeter.h`
- `src/meters/widgets/LevelMeter.cpp`
- `src/meters/widgets/GainReductionMeter.cpp`
- `src/meters/widgets/StereoMeter.cpp`

Behavior:
- own `MeterValueModel`
- timer drives `tick()` + `repaint()`
- accept snapshots via `pushSnapshot()`

### 6) Dev demo support

Added:
- `include/mdsp_ui/meters/dev/MeterDemoComponent.h`
- `src/meters/dev/MeterDemoComponent.cpp`
- `src/meters/dev/MeterDemoApp.cpp`

CMake options:
- `MDSP_UI_BUILD_WIDGETS` (default `ON`)
- `MDSP_UI_BUILD_METER_DEMO` (default `OFF`)
- `MDSP_UI_BUILD_METER_DEMO_APP` (default `OFF`, requires demo enabled)

README was updated in HQ (`shared/mdsp_ui/README.md`) with demo configure/build/run commands.

### 7) CMake wiring in HQ

Updated:
- `shared/mdsp_ui/CMakeLists.txt`

Result:
- logic + renderer always built
- widgets are optional by flag
- demo component/app are opt-in and non-shipping by default

## AnalyzerPro: What Still Needs To Be Implemented

AnalyzerPro still uses local plugin-side meter UI classes under `Source/ui/meters/*`.

Goal: route AnalyzerPro meter visuals through `mdsp_ui/meters`.

## Recommended Integration Steps (AnalyzerPro)

### Step 1: Create a snapshot bridge layer

Add a small adapter in AnalyzerPro that maps engine/processor meter outputs to `mdsp_ui::meters::MeterSnapshot`.

Suggested new file:
- `Source/dsp_adapters/MdspMeterSnapshotAdapter.h` (+ `.cpp` if needed)

Adapter responsibilities:
- read current AnalyzerPro meter values (peak/rms/gr/clip)
- convert to dB conventions expected by `MeterConfig`
- push snapshots to UI widgets on message thread boundary

Important:
- no APVTS inside `mdsp_ui`
- no UI operations from audio thread

### Step 2: Replace plugin meter widgets in main view

Current plugin UI meter files:
- `Source/ui/meters/MeterComponent.h`
- `Source/ui/meters/MeterComponent.cpp`
- `Source/ui/meters/MeterGroupComponent.h`
- `Source/ui/meters/MeterGroupComponent.cpp`

Integration approach:
- keep layout containers in AnalyzerPro (`MainView`, meter panel layout)
- replace rendering components with:
  - `mdsp_ui::meters::widgets::LevelMeter`
  - `mdsp_ui::meters::widgets::GainReductionMeter` (if used)
  - `mdsp_ui::meters::widgets::StereoMeter` (for dual channel views)

### Step 3: Theme wiring

From AnalyzerPro `UiContext`, pass theme into meter widgets:
- `meterWidget.setTheme(ui_.theme());`

Do not instantiate local `mdsp_ui::Theme` objects in plugin components.

### Step 4: Define meter configs in one place

Create a small config helper (plugin-side) for meter instances:
- input meter config (range/orientation/channels)
- output meter config
- GR meter config if needed

This keeps plugin policy local while `mdsp_ui` stays generic.

### Step 5: Remove duplicated plugin-side meter drawing logic

After successful replacement:
- delete old meter paint/ballistics code in `Source/ui/meters/*`
- keep only container/layout and snapshot wiring

### Step 6: Verification checklist

Build checks:
1. `mdsp_ui` target builds with default flags
2. AnalyzerPro builds and links without local meter renderer duplication
3. Optional: demo app builds when enabled

Runtime checks:
1. Input/output meters move correctly with audio
2. Hold behavior matches configured ballistics
3. Clip indicators latch/release as expected
4. Theme switches correctly recolor meters

Architecture checks:
1. no `APVTS` usage inside `mdsp_ui/meters/*`
2. no AnalyzerPro-specific includes inside `mdsp_ui/meters/*`
3. no audio-thread UI calls during snapshot publish

## Suggested AnalyzerPro Touch Points

Most likely files to modify first:
- `Source/ui/MainView.h`
- `Source/ui/MainView.cpp`
- `Source/ui/meters/MeterGroupComponent.h`
- `Source/ui/meters/MeterGroupComponent.cpp`
- `Source/dsp_adapters/AnalyzerSnapshotAdapter.h`
- `Source/PluginEditor.cpp`

## Definition Of Done For AnalyzerPro Integration

Integration is complete when:
- AnalyzerPro meter visuals are driven by `mdsp_ui/meters/widgets/*`
- snapshots are bridged from AnalyzerPro engine to `MeterSnapshot`
- no plugin-specific meter rendering logic remains duplicated
- build passes with HQ `mdsp_ui` as single source of meter UI truth

