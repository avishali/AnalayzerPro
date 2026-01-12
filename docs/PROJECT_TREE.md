# AnalyzerPro - Project Structure Tree

## Overview
AnalyzerPro is a JUCE-based audio analyzer plugin with real-time FFT, 1/3-octave bands, and log-frequency spectrum displays. The project follows a modular architecture with clear separation between audio processing, UI components, and hardware integration.

## Project Directory Tree

```
AnalyzerPro/
├── CMakeLists.txt                 # Main CMake build configuration
├── CMakePresets.json              # CMake preset configurations
├── README.md                      # Project documentation
├── CHANGES.md                     # Change log
├── CONTRIBUTING.md                # Contribution guidelines
│
├── Source/                        # Main source code directory
│   ├── PluginProcessor.h/cpp     # Core audio processor (APVTS, processBlock)
│   ├── PluginEditor.h/cpp        # Main plugin editor window
│   │
│   ├── analyzer/                  # FFT analysis engine
│   │   ├── AnalyzerEngine.h/cpp  # FFT computation, smoothing, peak hold
│   │   └── AnalyzerSnapshot.h    # Lock-free snapshot structure (audio→UI)
│   │
│   ├── audio/                     # Audio utilities
│   │   └── DeviceRoutingHelper.h/cpp  # Device routing utilities
│   │
│   ├── config/                    # Configuration
│   │   └── DevFlags.h            # Development flags/options
│   │
│   ├── control/                   # Parameter/control binding system
│   │   ├── ControlIds.h          # Enum: ControlId (AnalyzerMode, FftSize, etc.)
│   │   ├── ControlBinder.h/cpp   # Binds UI controls to APVTS parameters
│   │   ├── ControlSpecs.h/cpp    # Control specifications
│   │   ├── AnalyzerProParamIdMap.h/cpp  # Maps ControlId → APVTS param ID strings
│   │   └── AnalyzerProControlContext.h/cpp  # Control context management
│   │
│   ├── parameters/                # Parameter definitions
│   │   └── Parameters.h/cpp      # Parameter structure definitions
│   │
│   ├── hardware/                  # Hardware meter integration
│   │   ├── IHardwareMeterSink.h  # Interface for hardware meter sinks
│   │   ├── SoftwareMeterSink.h/cpp  # Software meter implementation
│   │   ├── HardwareMeterMapper.h/cpp  # Maps software meters to hardware
│   │   ├── PluginHardwareAdapter.h/cpp  # Plugin hardware adapter
│   │   └── PluginHardwareOutputAdapter.h/cpp  # Hardware output adapter
│   │
│   └── ui/                        # User interface components
│       ├── MainView.h/cpp        # Main UI container (layout: meters + scope + controls)
│       │
│       ├── analyzer/              # Analyzer display components
│       │   ├── AnalyzerDisplayView.h/cpp  # Analyzer view controller (timer, snapshot polling)
│       │   └── rta1_import/      # RTADisplay component (FFT/BANDS/LOG rendering)
│       │       ├── RTADisplay.h/cpp  # Spectrum display component
│       │       └── melechdsp-hq.code-workspace
│       │
│       ├── layout/                # UI layout components
│       │   ├── HeaderBar.h/cpp   # Top header (read-only mode label)
│       │   ├── FooterBar.h/cpp   # Bottom footer
│       │   └── ControlRail.h/cpp # Right-side controls panel (Mode, FFT Size, etc.)
│       │
│       ├── meters/                # Audio meter components
│       │   ├── MeterComponent.h/cpp  # Single channel meter
│       │   └── MeterGroupComponent.h/cpp  # Meter group (L/R input/output)
│       │
│       └── views/                 # Additional view components
│           ├── PhaseCorrelationView.h/cpp  # Phase correlation view
│           ├── PhaseScopePlaceholder.h/cpp # Phase scope placeholder
│           └── AnalyzerGridPlaceholder.h/cpp  # Analyzer grid placeholder
│
├── ui_core/                       # Shared UI core library
│   ├── CMakeLists.txt            # UI core CMake configuration
│   ├── include/                   # Public headers (8 files)
│   └── src/                       # Implementation (1 file)
│
├── third_party/                   # Third-party dependencies
│   └── melechdsp-hq/             # MelecDSP shared modules (git submodule)
│       └── [mdsp_core, mdsp_ui, mdsp_dsp]  # Shared MelecDSP libraries
│
├── docs/                          # Documentation
│   ├── ARCHITECTURE.md           # Architecture documentation
│   ├── SPEC.md                   # Specification
│   ├── BUILD_VERIFY.md           # Build verification guide
│   ├── CONTROL_IDS.md            # Control ID reference
│   ├── DIAGNOSTICS_INVARIANTS.md # Diagnostic invariants
│   ├── RENDER_AUDITS.md          # Rendering audit documentation
│   ├── AI_GOVERNANCE.md          # AI development workflow
│   ├── AI_WORKFLOW_DIAGRAM.md    # AI workflow diagram (markdown)
│   ├── HANDOFF_PACKAGE.txt       # Handoff package template
│   ├── HANDOFF_TEMPLATE.md       # Handoff template
│   ├── docs/
│   │   └── HANDOFF_PACKAGE_NEXT.txt
│   └── *.png, *.svg              # Workflow diagrams
│
├── PROMPTS/                       # AI prompt templates
│   ├── README.txt                # Prompt system documentation
│   ├── PROMPTS_INDEX.md          # Prompt index
│   ├── MASTER_CURSOR_PROMPT.txt  # Master Cursor prompt
│   ├── PRE_COMMIT_CHECKLIST.md   # Pre-commit checklist
│   ├── 00_SYSTEM_RULES.txt       # System rules
│   ├── 01_ENGINE/                # Engine prompts (7 files)
│   ├── 02_UI_ANALYZER/           # UI Analyzer prompts (7 files)
│   ├── 03_UI_CONTROLS/           # UI Controls prompts (4 files)
│   ├── 04_PERFORMANCE/           # Performance prompts (4 files)
│   ├── 05_REFACTOR_GUARDS/       # Refactor guard prompts (3 files)
│   └── TEMPLATES/                # Prompt templates (6 files)
│
├── scripts/                       # Build/utility scripts
│   ├── build.sh                  # Build script
│   ├── clean_build.sh            # Clean build script
│   ├── check-no-manual-axes.sh   # Check for manual axes
│   ├── check-no-warnings.sh      # Check for warnings
│   └── new_plugin.sh             # New plugin creation script
│
├── build/                         # Build artifacts (generated)
│   ├── debug/                    # Debug build
│   └── release/                  # Release build
│
└── build-xcode/                   # Xcode build artifacts (generated)
    ├── AnalyzerPro.xcodeproj/    # Xcode project
    └── [build artifacts]
```

## Key Components Overview

### Audio Processing Chain
```
PluginProcessor::processBlock()
  ├── Input meter measurement
  ├── AnalyzerEngine::pushAudioSamples()  # Audio → FFT
  │   ├── AnalyzerEngine::computeFFT()    # FFT computation
  │   ├── Smoothing (EMA)                 # Averaging
  │   └── Peak hold tracking              # Peak detection
  ├── AnalyzerEngine::publishSnapshot()   # Lock-free snapshot
  └── Output meter measurement
```

### UI Thread Chain
```
PluginEditor (MainView)
  ├── AnalyzerDisplayView::timerCallback()  # Poll snapshots
  │   ├── AnalyzerEngine::getLatestSnapshot()  # Lock-free read
  │   └── AnalyzerDisplayView::updateFromSnapshot()
  │       ├── Mode::FFT → RTADisplay::setFFTData()
  │       ├── Mode::BANDS → convertFFTToBands() → RTADisplay::setBandData()
  │       └── Mode::LOG → convertFFTToLog() → RTADisplay::setLogData()
  ├── ControlRail  # User controls (Mode, FFT Size, Averaging, etc.)
  ├── MeterGroupComponent (Input/Output meters)
  └── RTADisplay  # Spectrum rendering
```

### Control/Parameter Flow
```
UI Control (ControlRail)
  → ControlBinder::bindCombo/Toggle/Slider()
  → APVTS parameter change
  → PluginProcessor::parameterChanged()
  → AnalyzerEngine::setMode/setFftSize/setAveraging/etc.
  → Audio thread applies changes
```

## Key Classes

### AnalyzerEngine
- **Purpose**: Real-time FFT analysis engine (audio thread)
- **Key Methods**:
  - `prepare(sampleRate, blockSize)` - Initialize FFT
  - `pushAudioSamples(buffer)` - Feed audio data
  - `computeFFT()` - Perform FFT, smoothing, peak hold
  - `publishSnapshot()` - Publish lock-free snapshot (atomic sequence)
  - `getLatestSnapshot(snapshot)` - Read snapshot (UI thread, retry loop)
- **Modes**: FFT, BANDS (1/3-octave), LOG (log-frequency)

### AnalyzerDisplayView
- **Purpose**: UI controller for analyzer display
- **Key Methods**:
  - `timerCallback()` - Poll snapshots (30-60 Hz)
  - `updateFromSnapshot()` - Feed RTADisplay based on mode
  - `setMode()` - Change display mode (authoritative)
  - `convertFFTToBands()` - Convert FFT → 1/3-octave bands
  - `convertFFTToLog()` - Convert FFT → log-frequency bins

### RTADisplay
- **Purpose**: Generic spectrum display component
- **Modes**: FFT (linear), BANDS (1/3-octave), LOG (log-frequency)
- **Features**: Display Gain, Tilt compensation (Pink/White), Peak hold overlay

### PluginProcessor
- **Purpose**: Main audio processor
- **Features**:
  - APVTS (AudioProcessorValueTreeState) for parameters
  - Meter measurement (input/output, peak/RMS)
  - Hardware meter integration
  - Parameter change handling

### ControlRail
- **Purpose**: Right-side control panel
- **Controls**:
  - Mode (FFT/BANDS/LOG)
  - FFT Size (1024/2048/4096/8192)
  - Averaging (Off/50ms/100ms/250ms/500ms/1s)
  - Peak Hold (Enable/Disable)
  - Hold (Freeze decay)
  - Peak Decay (dB/sec)
  - Display Gain (-24..+24 dB)
  - Tilt (Flat/Pink/White)
  - dB Range (-60/-90/-120 dB)

## Build System

### CMake Configuration
- **Minimum CMake**: 3.22
- **JUCE**: Submodule or external
- **Targets**:
  - `AnalyzerPro` - Main plugin target
  - `ui_core` - UI core library (OBJECT)
  - Optional: `mdsp_core`, `mdsp_ui`, `mdsp_dsp` (from melechdsp-hq)

### Build Options
- `PLUGIN_DEV_MODE` - Fast iteration mode (limits formats, disables LTO)
- `UniversalBinary` - Build universal binary for macOS
- `CMAKE_BUILD_TYPE` - Debug/Release

## Thread Safety Model

### Audio Thread (Real-time Safe)
- `AnalyzerEngine::pushAudioSamples()`
- `AnalyzerEngine::computeFFT()`
- `AnalyzerEngine::publishSnapshot()` (writes atomic sequence)

### UI Thread
- `AnalyzerDisplayView::timerCallback()`
- `AnalyzerDisplayView::updateFromSnapshot()`
- `RTADisplay::paint()`
- Control callbacks → APVTS parameter changes

### Lock-free Communication
- `PublishedAnalyzerSnapshot` with atomic sequence counter
- Seqlock-style pattern: write sequence (release), read sequence (acquire), validate (seq1 == seq2)
- UI holds last valid frame on torn reads

## Parameters (APVTS)

| Parameter ID | Type | Range/Options | Default |
|--------------|------|---------------|---------|
| Mode | Choice | FFT, BANDS, LOG | FFT |
| FftSize | Choice | 1024, 2048, 4096, 8192 | 2048 |
| Averaging | Choice | Off, 50ms, 100ms, 250ms, 500ms, 1s | 100ms |
| PeakHold | Bool | true/false | true |
| Hold | Bool | true/false | false |
| PeakDecay | Float | 0.0..60.0 dB/s | 1.0 dB/s |
| DisplayGain | Float | -24.0..+24.0 dB | 0.0 dB |
| Tilt | Choice | Flat, Pink, White | Flat |
| DbRange | Choice | -60, -90, -120 dB | -120 dB |

## File Count Summary

- **Header files (.h)**: ~28
- **Source files (.cpp)**: ~24
- **Documentation**: ~15 markdown files
- **Prompt templates**: ~40+ text files
- **Scripts**: 5 shell scripts
- **Build artifacts**: Generated (not tracked)

## Key Design Principles

1. **Thread Safety**: Lock-free snapshot communication (atomic sequence)
2. **Mode Authority**: Single source of truth (ControlRail Mode dropdown)
3. **Smooth Updates**: UI holds last valid frame (no blinking/floor frames)
4. **Minimal Allocations**: Reuse buffers, no allocations in `computeFFT()`
5. **Correctness**: Proper JUCE real-only FFT buffer usage, correct magnitude extraction
6. **Separation of Concerns**: Audio thread (FFT) vs UI thread (rendering)
7. **Parameter Binding**: Centralized APVTS → ControlBinder → UI controls

---

**Generated**: 2024-12-19
**Project**: AnalyzerPro
**Type**: JUCE Audio Plugin (VST3/AU/Standalone)
