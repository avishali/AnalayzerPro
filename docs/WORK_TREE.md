# WORK_TREE.md — AnalyzerPro + SDK Ownership Map

Purpose: quick map of where each module/feature lives today between AnalyzerPro (product repo) and MelechDSP HQ (SDK repo).

## Repositories

- AnalyzerPro (product): `/Users/avishaylidani/DEV/GitHubRepo/MelechDSP/AnalyzerPro`
- SDK / HQ (shared): `/Users/avishaylidani/DEV/GitHubRepo/MelechDSP/melechdsp-hq`

## Ownership Rules

- Product-specific integration stays in AnalyzerPro.
- Reusable DSP engines/processors live in `mdsp_dsp` (HQ).
- Reusable UI rendering/widgets live in `mdsp_ui` (HQ).
- AnalyzerPro keeps thin adapters for SDK-owned types to avoid callsite churn.

## AnalyzerPro Tree (Owner = Product)

- `Source/PluginProcessor.*`: plugin lifecycle, APVTS wiring, processBlock orchestration.
- `Source/PluginEditor.*`: editor shell and product-level view composition.
- `Source/parameters/*`: parameter definitions/ranges/defaults.
- `Source/control/*`: control IDs, bindings, param ID maps, control context.
- `Source/presets/*`: preset + A/B state product logic.
- `Source/audio/*`: device routing/helpers.
- `Source/hardware/*`: hardware meter mapping/sinks/adapters.
- `Source/ui/*`: product layout/composition and product UX behavior.

## AnalyzerPro Adapter Layer (Owner = Product, Data/Engine = SDK)

- `Source/dsp_adapters/AnalyzerSnapshotAdapter.h`
  - aliases `mdsp_dsp::AnalyzerSnapshot` and `mdsp_dsp::PublishedAnalyzerSnapshot`.
- `Source/loudness/LoudnessAnalyzer.h`
  - aliases `mdsp_dsp::LoudnessAnalyzer` and `mdsp_dsp::LoudnessSnapshot`.
- `Source/analyzer/StereoScopeAnalyzer.h`
  - aliases `mdsp_dsp::StereoScopeAnalyzer`.
- `Source/analyzer/AnalyzerEngine.*`
  - thin adapter over `mdsp_dsp::AnalyzerEngine`, plus product-side orchestration.

## SDK Tree Consumed by AnalyzerPro (Owner = HQ)

### `shared/mdsp_dsp`

- Core utilities:
  - `include/mdsp_dsp/Smoother.h`
  - `include/mdsp_dsp/MeterBallistics.h`
- Analyzer:
  - `include/mdsp_dsp/analyzer/AnalyzerEngine.h`
  - `include/mdsp_dsp/analyzer/AnalyzerSnapshot.h`
- Loudness:
  - `include/mdsp_dsp/loudness/LoudnessAnalyzer.h`
- Scopes:
  - `include/mdsp_dsp/scopes/StereoScopeAnalyzer.h`
- Spectrogram:
  - `include/mdsp_dsp/spectrogram/SpectrogramAccumulator.h`
  - `include/mdsp_dsp/spectrogram/SpectrogramSettings.h`

### `shared/mdsp_ui`

- Reusable rendering/controllers/theme/tokens for analyzer + controls + RTA.
- AnalyzerPro composes these inside product UI classes.

### `shared/mdsp_core`

- Shared foundational utilities (e.g. queue/container/version/assert layers).

### `shared/mdsp_gui`

- Shared higher-level GUI DSP components (spectrum/spectrogram components/processors).

## Feature-to-Owner Quick Map

- FFT engine + snapshot publication: HQ (`mdsp_dsp::AnalyzerEngine`, `AnalyzerSnapshot`).
- Loudness (M/S/I + peak): HQ (`mdsp_dsp::LoudnessAnalyzer`).
- Stereo scope ring-buffer analyzer: HQ (`mdsp_dsp::StereoScopeAnalyzer`).
- APVTS parameter model + binding policy: AnalyzerPro.
- Main layout / branding / product UX behavior: AnalyzerPro.
- Host IO + hardware adapters: AnalyzerPro.

## Current Migration Status

Completed extractions used by AnalyzerPro:

1. MeterBallistics/Smoother -> HQ
2. AnalyzerSnapshot -> HQ
3. AnalyzerEngine -> HQ (AnalyzerPro thin adapter remains)
4. LoudnessAnalyzer -> HQ (AnalyzerPro alias header remains)
5. StereoScopeAnalyzer -> HQ (AnalyzerPro alias header remains)

## Build Note

AnalyzerPro is configured to use the main HQ repo path (not `third_party`) via `HQ_DIR` resolution in `CMakeLists.txt`.
