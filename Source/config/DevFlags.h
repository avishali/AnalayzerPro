#pragma once

//==============================================================================
/**
    Development feature flags and utilities.
    All flags are compile-time only with zero runtime cost.
*/

// Read PLUGIN_DEV_MODE compile definition
#ifndef PLUGIN_DEV_MODE
#define PLUGIN_DEV_MODE 0
#endif

//==============================================================================
// Feature Flags
//==============================================================================

#if PLUGIN_DEV_MODE
    // Dev mode defaults
    #ifndef DEV_DIAGNOSTICS_PANEL
    #define DEV_DIAGNOSTICS_PANEL 1
    #endif

    #ifndef DEV_HEAVY_LOGGING
    #define DEV_HEAVY_LOGGING 0
    #endif

    #ifndef DEV_ANALYZER_VISUALS
    #define DEV_ANALYZER_VISUALS 1
    #endif
#else
    // Release mode defaults
    #ifndef DEV_DIAGNOSTICS_PANEL
    #define DEV_DIAGNOSTICS_PANEL 0
    #endif

    #ifndef DEV_HEAVY_LOGGING
    #define DEV_HEAVY_LOGGING 0
    #endif

    #ifndef DEV_ANALYZER_VISUALS
    #define DEV_ANALYZER_VISUALS 0
    #endif
#endif

//==============================================================================
// Dev Logging Macro
//==============================================================================

#if DEV_HEAVY_LOGGING
    #include <juce_core/juce_core.h>
    #define DEV_LOG(x) juce::Logger::writeToLog(x)
#else
    #define DEV_LOG(x) ((void)0)  // Compiles out completely
#endif
