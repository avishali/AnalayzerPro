#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <mdsp_ui/AxisInteraction.h>
#include <mdsp_ui/AxisHoverController.h>
#include <mdsp_ui/PeakSnapController.h>
#include "RTAGeometry.h"
#include "RTADisplayModel.h"
#include <functional>
#include <cstdint>

class RTADisplayModel;
class RTAGeometry;

//==============================================================================
/**
    Controller for RTA display interaction: mouse, wheel, hover, drag, timer smoothing.
    Owns interaction state only. Does NOT paint.
    Reads RTAGeometry and RTADisplayModel for mapping and data.
*/
class RTADisplayController
{
public:
    using RenderState = RTADisplayModel::RenderState;

    struct Callbacks
    {
        std::function<void()> requestRepaint;
        std::function<void(int, int, int, int)> requestRepaintRect;
    };

    RTADisplayController (RTADisplayModel& model, RTAGeometry& geometry);
    ~RTADisplayController() = default;

    void setCallbacks (Callbacks cbs);
    void setDisplayGainDb (float db);

    void onMouseMove (const juce::MouseEvent& e);
    void onMouseExit (const juce::MouseEvent& e);
    void onMouseDown (const juce::MouseEvent& e);
    void onMouseDrag (const juce::MouseEvent& e);
    void onMouseUp (const juce::MouseEvent& e);
    void onMouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel);
    void onTimerTick();
    void onLayoutChanged();
    void onViewModeChanged();
    void onFftDataChanged();
    void onStructuralReset();

    /** Clear selection when mouse-up was stolen (e.g. by popup). Call from paint when no button down. */
    void clearSelectionIfStuck();

    // Getters for paint usage
    bool isHoverActive() const { return hoveredBandIndex_ >= 0; }
    int getHoveredBandIndex() const { return hoveredBandIndex_; }
    bool isFftHoverActive() const { return fftHoverActive_; }
    float getFftHoverMouseXpx() const { return fftHoverMouseXpx_; }
    float getFftHoverSnappedXpx() const { return fftHoverSnappedXpx_; }
    float getFftHoverSnappedYpx() const { return fftHoverSnappedYpx_; }
    float getFftHoverSnappedFreq() const { return fftHoverSnappedFreq_; }
    float getFftHoverSnappedDb() const { return fftHoverSnappedDb_; }
    bool isFftHoverDbValid() const { return fftHoverDbValid_; }
    const juce::String& getFftHoverReadoutText() const { return fftHoverReadoutText_; }
    const juce::String& getFftHoverFreqText() const { return fftHoverFreqText_; }
    float getFftHoverReadoutWidth() const { return fftHoverReadoutWidth_; }
    bool isHoverDbHasValue() const { return hoverDbHasValue_; }
    float getHoverDbTarget() const { return hoverDbTarget_; }
    float getHoverDbSmooth() const { return hoverDbSmooth_; }
    float getHoverYSmoothPx() const { return hoverYSmoothPx_; }

    const mdsp_ui::AxisHoverController& getFreqHover() const { return freqHover_; }
    const mdsp_ui::AxisHoverController& getDbHover() const { return dbHover_; }
    const mdsp_ui::PeakSnapController& getPeakSnap() const { return peakSnap_; }

    bool isSelectionActive() const { return selectionActive_; }
    const juce::Rectangle<int>& getSelectionRect() const { return selectionRect_; }

#if JUCE_DEBUG
    bool getUseEnvelopeDecimator() const { return useEnvelopeDecimator_; }
#endif

private:
    RTADisplayModel& model_;
    RTAGeometry& geometry_;
    Callbacks callbacks_;
    float displayGainDb_ = 0.0f;

    struct FreqAxisConfig
    {
        juce::Array<mdsp_ui::AxisTick> ticks;
        mdsp_ui::AxisMapping mapping;
        mdsp_ui::AxisSnapOptions snap;
        mdsp_ui::AxisHoverControllerStyle style;
    };
    struct DbAxisConfig
    {
        juce::Array<mdsp_ui::AxisTick> ticks;
        mdsp_ui::AxisMapping mapping;
        mdsp_ui::AxisSnapOptions snap;
        mdsp_ui::AxisHoverControllerStyle style;
    };

    FreqAxisConfig buildFreqAxisConfig (const RenderState& s) const;
    DbAxisConfig buildDbAxisConfig (const RenderState& s) const;
    float getDbAtPixelX (float xPx, const RenderState& s) const;

    void requestRepaintMaybeRect (bool needsRepaint, const RenderState& s);

    int hoveredBandIndex_ = -1;
    bool fftHoverActive_ = false;
    int fftHoverBinIndex_ = -1;
    float fftHoverMouseXpx_ = 0.0f;
    float fftHoverSnappedXpx_ = 0.0f;
    float fftHoverSnappedYpx_ = 0.0f;
    float fftHoverSnappedFreq_ = 0.0f;
    float fftHoverSnappedDb_ = 0.0f;
    bool fftHoverDbValid_ = false;
    juce::String fftHoverReadoutText_;
    juce::String fftHoverFreqText_;
    float fftHoverReadoutWidth_ = 0.0f;
    float hoverDbTarget_ = 0.0f;
    float hoverDbSmooth_ = 0.0f;
    bool hoverDbHasValue_ = false;
    float hoverYSmoothPx_ = 0.0f;
    double hoverLastSmoothTimeSec_ = 0.0;

    mdsp_ui::AxisHoverController freqHover_;
    mdsp_ui::AxisHoverController dbHover_;
    mdsp_ui::PeakSnapController peakSnap_;

    juce::Rectangle<int> selectionRect_;
    bool selectionActive_ = false;

#if JUCE_DEBUG
    bool useEnvelopeDecimator_ = false;
#endif

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RTADisplayController)
};
