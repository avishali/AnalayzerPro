# AnalyzerPro — Folder Structure

Quick reference for the repository directory layout. For architecture and file-by-file details, see `docs/PROJECT_TREE.md` and `docs/ARCHITECTURE.md`.

---

## Root layout

```
AnalyzerPro/
├── CMake/                      # CMake helpers
├── CMakeLists.txt              # Main build configuration
├── CMakePresets.json           # Build presets
├── FOLDER_STRUCTURE.md         # This file
├── README.md
├── CHANGES.md
│
├── Source/                     # Plugin source
├── third_party/                # Dependencies (melechdsp-hq submodule)
├── docs/                       # Documentation
├── scripts/                    # Build, install, release scripts
├── installer/                  # Built installers (DMG, PKG, ZIP)
│
├── build/                      # Release build output
├── build-debug/                # Debug build output
│
├── PROMPTS/                    # AI prompts, missions, runbooks
├── .cursorrules                # Cursor AI rules
├── .claude/                    # Claude project config
└── .vscode/                    # VS Code config
```

---

## Source/ — Engine + UI

```
Source/
├── PluginProcessor.h/cpp       # Main audio processor (APVTS, processBlock)
├── PluginEditor.h/cpp          # Main plugin editor window
│
├── analyzer/                   # FFT / spectrum analysis engine
│   ├── AnalyzerEngine.h/cpp
│   ├── AnalyzerSnapshot.h
│   └── StereoScopeAnalyzer.h/cpp
│
├── audio/                      # Audio utilities
│   ├── DeviceRoutingHelper.h/cpp
│   └── IStereoScopeSink.h
│
├── config/
│   └── DevFlags.h
│
├── control/                    # Parameter binding and control IDs
│   ├── ControlIds.h
│   ├── ControlBinder.h/cpp
│   ├── ControlSpecs.h/cpp
│   ├── AnalyzerProParamIdMap.h/cpp
│   └── AnalyzerProControlContext.h/cpp
│
├── parameters/
│   └── Parameters.h/cpp
│
├── hardware/                   # Hardware meter integration
│   ├── IHardwareMeterSink.h
│   ├── SoftwareMeterSink.h/cpp
│   └── HardwareMeterMapper.h/cpp
│
├── presets/                    # Preset and A/B state
│   ├── PresetManager.h/cpp
│   └── ABStateManager.h/cpp
│
├── loudness/                    # Loudness analysis (LUFS, K-weighting)
│   └── LoudnessAnalyzer.h/cpp
│
└── ui/                         # User interface
    ├── MainView.h/cpp
    ├── ControlPanel.h/cpp
    ├── DebugGridOverlay.h
    │
    ├── analyzer/               # Analyzer displays
    │   ├── AnalyzerDisplayView.h/cpp
    │   ├── StereoScopeView.h/cpp
    │   └── RTADisplay.h/cpp      # FFT / BANDS / LOG spectrum display
    │
    ├── layout/
    │   ├── HeaderBar.h/cpp
    │   ├── FooterBar.h/cpp
    │   ├── ControlRail.h/cpp
    │   ├── DraggableParamValueLabel.h/cpp
    │   ├── LayoutConstants.h
    │   └── PixelSnap.h
    │
    ├── loudness/
    │   └── LoudnessNumericPanel.h/cpp
    │
    ├── meters/
    │   ├── MeterComponent.h/cpp
    │   ├── MeterGroupComponent.h/cpp
    │   ├── StereoScopeComponent.h/cpp
    │   └── PhaseFanScopeComponent.h/cpp
    │
    └── tooltips/
        ├── TooltipData.h
        ├── TooltipManager.h/cpp
        └── TooltipOverlayComponent.h/cpp
```

---

## third_party/melechdsp-hq/shared/ — Shared libraries

```
third_party/melechdsp-hq/shared/
├── mdsp_core/       # Core containers and utilities
├── mdsp_ui/         # Shared UI controls and theme
├── mdsp_dsp/        # Shared DSP (spectrogram, etc.)
└── mdsp_gui/        # Shared GUI components
```

---

## docs/

```
docs/
├── ARCHITECTURE.md
├── PROJECT_TREE.md
├── SIGNAL_FLOW.md
├── HARDWARE_ADAPTER.md
├── RELEASE_GUIDE.md
├── ... (other .md and .txt docs)
```

---

## scripts/

```
scripts/
├── build.sh              # Dev build
├── build_release.sh      # Release build
├── clean_build.sh
├── create_installer.sh
├── create_simple_installer.sh
├── sign_and_notarize.sh
├── run_standalone.sh
├── check-no-manual-axes.sh
└── check-no-warnings.sh
```

---

## PROMPTS/

```
PROMPTS/
├── CHECKLISTS/
├── INDEX/
├── MISSIONS/
│   ├── 01_ENGINE/
│   ├── 02_UI_ANALYZER/
│   ├── 03_UI_CONTROLS/
│   ├── 04_PERFORMANCE/
│   └── 05_REFACTOR_GUARDS/
├── RUNBOOKS/
├── SYSTEM/
└── TEMPLATES/
```
