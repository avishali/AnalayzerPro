# Cursor task — dedicated Peak-Hold Decay time (separate from the all-trace Release)

Today the Metal peak-hold decay (Hold OFF mode) reuses `frame.rmsReleaseMs`, which is the
`PeakDecay` param — that also drives the spectrum/all-trace release. The user wants the **peak-hold**
to have its OWN decay-time control, independent of the trace Release. Add a dedicated parameter +
control and wire it into the Metal peak-hold decay.

Mirror the existing `analyzerShowPeak` plumbing pattern (param → ControlId → param-map → UI control →
MainView read → AnalyzerDisplayView member → Metal frame field).

## 1. Parameter — `Source/PluginProcessor.cpp`
Add (near the other analyzer params):
```cpp
params.push_back (std::make_unique<juce::AudioParameterFloat> (
    "PeakHoldDecay", "Peak Hold Decay",
    juce::NormalisableRange<float> (200.0f, 10000.0f, 1.0f, 0.4f),  // ms, skewed for fine low end
    2000.0f));   // default 2 s
```

## 2. ControlId + param map
- `Source/control/ControlIds.h`: add `PeakHoldDecayTime`.
- `Source/control/AnalyzerProParamIdMap.cpp`: `m[ControlId::PeakHoldDecayTime] = "PeakHoldDecay";`

## 3. UI control
Add a small rotary/slider labelled "Hold Decay" (ms) RIGHT NEXT TO the existing Hold button + Release
control (find where `AnalyzerHoldPeaks` / the Release knob are laid out — footer `FooterBar` and/or
`SettingsPopupPanel`/`ControlRail`) and bind it to `ControlId::PeakHoldDecayTime`. Match the existing
knob/slider style and labelling. It only affects the peak-hold ceiling, not the traces.

## 4. Plumb into the Metal frame
- `Source/ui/analyzer/metal/MetalHostShared.h` `struct MetalAnalyzerFrame`: add
  `float peakHoldDecayMs = 2000.0f;`
- `Source/ui/analyzer/AnalyzerDisplayView.{h,cpp}`: add member `float peakHoldDecayMs_ = 2000.0f;`
  and a setter `setPeakHoldDecayMs(float)`. Where the frame is built (`fillMetalAnalyzerFrame`), set
  `frame.peakHoldDecayMs = peakHoldDecayMs_;`
- `Source/ui/MainView.cpp`: add `apvts` listener for `"PeakHoldDecay"`, and in `parameterChanged`
  handle it → `analyzerView_.setPeakHoldDecayMs (newValue);` (also set it once on init alongside the
  other trace params).

## 5. Use it in the Metal peak-hold latch — `MetalHost.mm` `updateAnalyzerPipelineFromSnapshot`
Change the decay-rate source from `frame.rmsReleaseMs` to the new dedicated field:
```objcpp
const float holdReleaseMs      = juce::jmax (1.0f, frame.peakHoldDecayMs);   // was frame.rmsReleaseMs
const float holdDecayDbPerSec  = 60000.0f / holdReleaseMs;
const float holdDecayThisFrame = holdDecayDbPerSec * static_cast<float> (juce::jmax (0.0, renderDtSeconds));
```
(Leave Hold ON = infinite latch unchanged. The trace Release / `PeakDecay` now only affects the
spectrum traces, not the peak-hold.)

## Acceptance
- A new "Hold Decay" control sits by the Hold button; changing it changes ONLY how fast the
  peak-hold ceiling falls in decay mode (Hold off). The trace Release no longer affects peak-hold.
- Hold ON still freezes infinitely until Reset. Persists via APVTS (saved by "Save as Default").
