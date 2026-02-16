#pragma once

#include <juce_graphics/juce_graphics.h>
#include <mdsp_ui/Theme.h>
#include <mdsp_ui/analyzer/AnalyzerRenderer.h>
#include <mdsp_ui/analyzer/AnalyzerRenderState.h>
#include <functional>
#include "RTAEnums.h"
#include "RTADisplayModel.h"
#include "RTAGeometry.h"
#include "RTADisplayController.h"

//==============================================================================
/**
    Renderer for RTA display drawing/painting logic.
    Owns all paint entry points and drawing helpers.
    Does NOT own data, geometry, or interaction state (reads from model/geometry/controller).
*/
class RTADisplayRenderer
{
public:
    RTADisplayRenderer() = default;
    ~RTADisplayRenderer() = default;

    /** Main paint entry point - renders entire display */
    void paint (juce::Graphics& g,
                const RTADisplayModel& model,
                const RTAGeometry& geometry,
                const RTADisplayController& controller,
                const mdsp_ui::Theme& theme,
                float displayGainDb,
                rta::TiltMode tiltMode,
                const RTADisplayModel::TraceConfig& traceConfig,
                juce::Rectangle<int> bounds,
                std::function<mdsp_ui::AnalyzerRenderState()> getRenderState);

    /** Grid and axis rendering (used by refreshBackground callback) */
    void drawGrid (juce::Graphics& g,
                   const RTADisplayModel::RenderState& s,
                   const RTAGeometry& geometry,
                   const mdsp_ui::Theme& theme,
                   float displayGainDb);

private:
    using RenderState = RTADisplayModel::RenderState;

    // Mode-specific paint methods
    void paintFFTMode (juce::Graphics& g,
                       const RenderState& s,
                       const RTAGeometry& geometry,
                       const RTADisplayController& controller,
                       const mdsp_ui::Theme& theme,
                       float displayGainDb,
                       rta::TiltMode tiltMode,
                       const RTADisplayModel::TraceConfig& traceConfig,
                       RTADisplayModel& modelMutable); // For ensurePathsBuilt

    void paintBandsMode (juce::Graphics& g,
                         const RenderState& s,
                         const RTAGeometry& geometry,
                         const RTADisplayController& controller,
                         const mdsp_ui::Theme& theme,
                         float displayGainDb);

    void paintLogMode (juce::Graphics& g,
                       const RenderState& s,
                       const RTAGeometry& geometry,
                       const RTADisplayController& controller,
                       const mdsp_ui::Theme& theme,
                       float displayGainDb);

    // Overlay rendering
    void paintInteractionOverlays (juce::Graphics& g,
                                    const RenderState& s,
                                    const RTAGeometry& geometry,
                                    const RTADisplayController& controller,
                                    const mdsp_ui::Theme& theme,
                                    float displayGainDb);

    void paintLegacyTraceOverlays (juce::Graphics& g,
                                    const RenderState& s,
                                    const RTAGeometry& geometry,
                                    const RTADisplayModel& model,
                                    const mdsp_ui::Theme& theme,
                                    const RTADisplayModel::TraceConfig& traceConfig);

    // Helper: draw silk trace with glow
    static void drawSilkTrace (juce::Graphics& g,
                                const juce::Path& path,
                                juce::Colour coreColour,
                                float baseThicknessPx,
                                float viewportWidth,
                                bool isPeakTrace,
                                float energyMul,
                                bool useShimmer);

    // Helper: compute BS468 weighting
    static float getBS468WeightingDb (float freqHz);
};
