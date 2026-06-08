#pragma once

// Centralized UI update rates. These are uniform across formats: measurement
// showed that throttling scope/meter rates in AAX gave no playback-fps benefit
// (Pro Tools caps message-thread UI at ~13 fps during playback regardless), and
// the format-specific path that selects tick source lives in AnalyzerDisplayView
// as a runtime wrapperType check - NOT a compile-time JucePlugin_Build_AAX guard
// (which is true for all formats in this single shared-code build).
//
// ANALYZERPRO_MAX_RENDER_HZ remains overridable from CMake for experiments.

#if !defined(ANALYZERPRO_MAX_RENDER_HZ)
 #define ANALYZERPRO_MAX_RENDER_HZ 60
#endif

namespace AnalyzerPro
{
namespace UiRates
{
static constexpr int kAnalyzerDataHz      = 30;  // snapshot pump + timer-tick render rate
static constexpr int kAnalyzerMaxRenderHz = ANALYZERPRO_MAX_RENDER_HZ; // VBlank interpolation cap
// Feed faster than CPU repaint so the Metal 60 Hz publish path has fresh meter render state;
// CPU meter components still repaint at kMeterHz to avoid doubling non-Metal paint cost.
static constexpr int kMeterFeedHz         = 60;
static constexpr int kMeterHz             = 30;
// Feed faster than CPU repaint so the Metal 60 Hz publish path has fresh point-cloud data;
// CPU scope components still refresh at kScopeHz to avoid doubling non-Metal paint cost.
static constexpr int kScopeFeedHz         = 60;
static constexpr int kScopeHz             = 30;
static constexpr int kNumericLabelHz      = 15;
}
}
