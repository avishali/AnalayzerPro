#pragma once

namespace AnalyzerPro
{
namespace Layout
{
    constexpr int outerPadding       = 10;  // thin outer frame (was 24)
    constexpr int gutterGap          = 16;
    constexpr int topBarHeight       = 32;
    constexpr int footerHeight       = 32;
    constexpr int meterRailWidth     = 106;  // room for meters (48+8+48) + clip LED on right channel
    constexpr int meterRailHeight    = 0;   // max height for left/right meter strips (0 = use full content height)
    constexpr int railMinWidth       = 240;
    constexpr int railNormalWidth    = 260;
    constexpr int railWideWidth      = 300;
    constexpr int bottomAreaMinHeight = 220;

    constexpr int compactBreakpoint  = 1100;  // Below this = Compact; at min editor 1100 we use Normal
    constexpr int wideBreakpoint     = 1600;

    // Stereo scope: square size cap and aspect (single source of truth for layout)
    constexpr int kScopeMaxSize      = 360;   // square size cap (adjust only if needed)
    constexpr float kScopeAspect     = 1.0f;  // MUST remain 1.0f (square)

    // Bottom area fixed widths
    constexpr int kLoudnessW         = 220;   // fixed loudness panel width (never varies with height)
}
}
