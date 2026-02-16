#include "RTADisplayController.h"
#include <mdsp_ui/AxisInteraction.h>
#include <cmath>
#include <cstdint>
#include <algorithm>
#include <limits>

RTADisplayController::RTADisplayController (RTADisplayModel& model, RTAGeometry& geometry)
    : model_ (model)
    , geometry_ (geometry)
    , freqHover_ ({})
    , dbHover_ ({})
    , peakSnap_ ([]() {
        mdsp_ui::PeakSnapStyle style;
        style.snapPx = 8.0f;
        style.releasePx = 16.0f;
        style.searchRadiusPx = 20.0f;
        style.epsPosPx = 0.5f;
        style.epsValue = 0.1f;
        return style;
    }())
{
}

void RTADisplayController::setCallbacks (Callbacks cbs)
{
    callbacks_ = std::move (cbs);
}

void RTADisplayController::setDisplayGainDb (float db)
{
    displayGainDb_ = db;
}

void RTADisplayController::requestRepaintMaybeRect (bool needsRepaint, const RenderState& s)
{
    if (!needsRepaint) return;
    if (s.viewMode == 0 && callbacks_.requestRepaintRect)
    {
        const int px = static_cast<int> (geometry_.getPlotAreaLeft());
        const int py = static_cast<int> (geometry_.getPlotAreaTop());
        const int pw = static_cast<int> (geometry_.getPlotAreaWidth()) + 2;
        const int ph = static_cast<int> (geometry_.getPlotAreaHeight()) + 2;
        callbacks_.requestRepaintRect (px, py, pw, ph);
    }
    else if (callbacks_.requestRepaint)
    {
        callbacks_.requestRepaint();
    }
}

float RTADisplayController::getDbAtPixelX (float xPx, const RenderState& s) const
{
    if (geometry_.getPlotAreaWidth() <= 0.0f || s.sampleRate <= 0.0 || s.fftSize <= 0 || s.fftDb.empty())
        return -200.0f;
    const std::vector<float>* data = (!s.fftPeakDb.empty() && s.fftPeakDb.size() == s.fftDb.size())
        ? &s.fftPeakDb : &s.fftDb;
    const size_t numBins = data->size();
    const float binWidthHz = static_cast<float> (s.sampleRate) / static_cast<float> (s.fftSize);
    const float logMin = std::log10 (s.minHz);
    const float logMax = std::log10 (s.maxHz);
    const float logRange = logMax - logMin;
    if (logRange <= 0.0f) return -200.0f;

    auto xToFreq = [&](float px) -> float {
        const float norm = (px - geometry_.getPlotAreaLeft()) / geometry_.getPlotAreaWidth();
        return std::pow (10.0f, juce::jlimit (logMin, logMax, logMin + norm * logRange));
    };

    const float x0 = xPx;
    const float x1 = xPx + 1.0f;
    const float freqStart = xToFreq (x0);
    const float freqEnd = xToFreq (x1);
    const float binStartF = freqStart / binWidthHz;
    const float binEndF = freqEnd / binWidthHz;

    if ((binEndF - binStartF) < 1.0f)
    {
        const float freqCenter = xToFreq (x0 + 0.5f);
        const float exactBin = freqCenter / binWidthHz;
        const size_t idx = static_cast<size_t> (exactBin);
        const float frac = exactBin - static_cast<float> (idx);
        if (idx < numBins - 1)
        {
            float v1 = (*data)[idx];
            float v2 = (*data)[idx + 1];
            if (!std::isfinite (v1)) v1 = -200.0f;
            if (!std::isfinite (v2)) v2 = -200.0f;
            return v1 * (1.0f - frac) + v2 * frac;
        }
        if (idx < numBins)
            return std::isfinite ((*data)[idx]) ? (*data)[idx] : -200.0f;
        return -200.0f;
    }

    size_t b0 = juce::jlimit ((size_t)0, numBins - 1, static_cast<size_t> (binStartF));
    size_t b1 = juce::jlimit ((size_t)0, numBins, static_cast<size_t> (std::ceil (binEndF)));
    b1 = std::max (b1, b0 + 1);
    b1 = std::min (b1, numBins);
    float maxVal = -200.0f;
    for (size_t k = b0; k < b1; ++k)
    {
        const float val = (*data)[k];
        if (std::isfinite (val) && val > maxVal) maxVal = val;
    }
    return maxVal;
}

RTADisplayController::FreqAxisConfig RTADisplayController::buildFreqAxisConfig (const RenderState& s) const
{
    FreqAxisConfig config;
    const float freqGridPoints[] = { 20.0f, 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f, 20000.0f };
    for (float freq : freqGridPoints)
    {
        if (freq >= s.minHz && freq <= s.maxHz)
        {
            const float x = geometry_.freqHzToXPx (freq);
            const float posPx = x - geometry_.getPlotAreaLeft();
            juce::String label;
            if (freq >= 1000.0f)
                label = juce::String (freq / 1000.0f, 1) + "k";
            else
                label = juce::String (static_cast<int> (freq));
            auto isMajorFreq = [](float f)
            {
                const float majors[] = { 20.0f, 100.0f, 1000.0f, 10000.0f, 20000.0f };
                for (float m : majors)
                    if (std::abs (f - m) < 0.1f) return true;
                return false;
            };
            const bool isMajor = isMajorFreq (freq);
            config.ticks.add ({ posPx, label, isMajor });
        }
    }
    config.mapping.scale = mdsp_ui::AxisScale::Log10;
    config.mapping.minValue = s.minHz;
    config.mapping.maxValue = s.maxHz;
    config.snap.mode = mdsp_ui::SnapMode::NearestLabelledTick;
    config.snap.maxSnapDistPx = 12.0f;
    config.style.snap = config.snap;
    config.style.epsPosPx = 0.5f;
    config.style.epsValue = 0.1f;
    return config;
}

RTADisplayController::DbAxisConfig RTADisplayController::buildDbAxisConfig (const RenderState& s) const
{
    DbAxisConfig config;
    for (int db = static_cast<int> (s.topDb); db >= static_cast<int> (s.bottomDb); db -= 6)
    {
        const float y = geometry_.dbToYPx (static_cast<float> (db) + displayGainDb_);
        if (y >= geometry_.getPlotAreaTop() && y <= geometry_.getPlotAreaTop() + geometry_.getPlotAreaHeight())
        {
            const float posPx = y - geometry_.getPlotAreaTop();
            const bool isMajor = (db % 12 == 0);
            juce::String label = isMajor ? (juce::String (db) + " dB") : juce::String();
            config.ticks.add ({ posPx, label, isMajor });
        }
    }
    config.mapping.scale = mdsp_ui::AxisScale::Linear;
    config.mapping.minValue = s.bottomDb;
    config.mapping.maxValue = s.topDb;
    config.snap.mode = mdsp_ui::SnapMode::NearestLabelledTick;
    config.snap.maxSnapDistPx = 12.0f;
    config.style.snap = config.snap;
    config.style.epsPosPx = 0.5f;
    config.style.epsValue = 0.1f;
    return config;
}

void RTADisplayController::onTimerTick()
{
    if (!fftHoverActive_)
        return;
    const double nowSec = juce::Time::getMillisecondCounterHiRes() * 0.001;
    if (!hoverDbHasValue_)
    {
        hoverLastSmoothTimeSec_ = nowSec;
        return;
    }
    double dtSeconds = nowSec - hoverLastSmoothTimeSec_;
    dtSeconds = juce::jlimit (0.0, 0.1, dtSeconds);
    hoverLastSmoothTimeSec_ = nowSec;
    if (dtSeconds <= 0.0)
        return;
    constexpr float tau = 0.08f;
    float alpha = 1.0f - std::exp (static_cast<float> (-dtSeconds / static_cast<double> (tau)));
    alpha = juce::jlimit (0.02f, 0.35f, alpha);
    hoverDbSmooth_ += alpha * (hoverDbTarget_ - hoverDbSmooth_);
    const float dbDiff = std::abs (hoverDbTarget_ - hoverDbSmooth_);
    const auto& s = model_.getState();
    if (dbDiff < 0.05f)
    {
        hoverDbSmooth_ = hoverDbTarget_;
        hoverYSmoothPx_ = geometry_.dbToYPx (hoverDbSmooth_ + displayGainDb_);
        if (std::isfinite (hoverYSmoothPx_))
        {
            fftHoverReadoutText_ = fftHoverFreqText_ + "  " + juce::String (hoverDbSmooth_, 1) + " dB";
            juce::GlyphArrangement ga;
            ga.addFittedText (juce::FontOptions().withHeight (10.0f), fftHoverReadoutText_,
                              0.0f, 0.0f, 10000.0f, 10.0f, juce::Justification::left, 1);
            fftHoverReadoutWidth_ = ga.getBoundingBox (0, -1, true).getWidth();
        }
        requestRepaintMaybeRect (true, s);
        return;
    }
    hoverYSmoothPx_ = geometry_.dbToYPx (hoverDbSmooth_ + displayGainDb_);
    if (!std::isfinite (hoverYSmoothPx_))
        return;
    fftHoverReadoutText_ = fftHoverFreqText_ + "  " + juce::String (hoverDbSmooth_, 1) + " dB";
    {
        juce::GlyphArrangement ga;
        ga.addFittedText (juce::FontOptions().withHeight (10.0f), fftHoverReadoutText_,
                          0.0f, 0.0f, 10000.0f, 10.0f, juce::Justification::left, 1);
        fftHoverReadoutWidth_ = ga.getBoundingBox (0, -1, true).getWidth();
    }
    requestRepaintMaybeRect (true, s);
}

void RTADisplayController::onLayoutChanged()
{
    fftHoverActive_ = false;
    hoverDbHasValue_ = false;
    hoverLastSmoothTimeSec_ = 0.0;
}

void RTADisplayController::onViewModeChanged()
{
    hoveredBandIndex_ = -1;
    fftHoverActive_ = false;
    hoverDbHasValue_ = false;
    hoverLastSmoothTimeSec_ = 0.0;
}

void RTADisplayController::onFftDataChanged()
{
    const auto& s = model_.getState();
    if (fftHoverActive_ && fftHoverBinIndex_ >= 0 && static_cast<size_t> (fftHoverBinIndex_) < s.fftDb.size())
    {
        const float dbVal = getDbAtPixelX (fftHoverMouseXpx_, s);
        const bool dbValid = s.hasValidSpectrumFrame && std::isfinite (dbVal) && dbVal > -200.0f;
        const float clampedDb = juce::jlimit (s.bottomDb, s.topDb + 18.0f, dbValid ? dbVal : s.bottomDb);
        fftHoverSnappedYpx_ = dbValid ? geometry_.dbToYPx (clampedDb + displayGainDb_) : (geometry_.getPlotAreaTop() + geometry_.getPlotAreaHeight() * 0.5f);
        if (dbValid)
        {
            hoverDbTarget_ = clampedDb;
            hoverDbSmooth_ = clampedDb;
            hoverYSmoothPx_ = fftHoverSnappedYpx_;
            fftHoverReadoutText_ = fftHoverFreqText_ + "  " + juce::String (clampedDb, 1) + " dB";
            juce::GlyphArrangement ga;
            ga.addFittedText (juce::FontOptions().withHeight (10.0f), fftHoverReadoutText_,
                              0.0f, 0.0f, 10000.0f, 10.0f, juce::Justification::left, 1);
            fftHoverReadoutWidth_ = ga.getBoundingBox (0, -1, true).getWidth();
            hoverDbHasValue_ = true;
        }
        else
        {
            hoverDbHasValue_ = false;
        }
    }
}

void RTADisplayController::onStructuralReset()
{
    hoveredBandIndex_ = -1;
    fftHoverActive_ = false;
    hoverDbHasValue_ = false;
    hoverLastSmoothTimeSec_ = 0.0;
}

void RTADisplayController::clearSelectionIfStuck()
{
    if (selectionActive_)
    {
        selectionActive_ = false;
        selectionRect_ = {};
    }
}

void RTADisplayController::onMouseMove (const juce::MouseEvent& e)
{
    const auto& state = model_.getState();
    const float x = static_cast<float> (e.x);
    const float y = static_cast<float> (e.y);
    int newHovered = -1;
    bool needsRepaint = false;

    if (state.viewMode == 2)
    {
        if (!geometry_.isGeometryValid() || geometry_.getBandGeometry().empty() || state.bandsDb.empty() || state.bandCentersHz.empty() ||
            state.bandsDb.size() != state.bandCentersHz.size() || geometry_.getBandGeometry().size() != state.bandCentersHz.size())
        {
            hoveredBandIndex_ = -1;
            if (freqHover_.deactivate() || dbHover_.deactivate())
                needsRepaint = true;
            requestRepaintMaybeRect (needsRepaint, state);
            return;
        }
        newHovered = geometry_.findNearestBand (x);
        newHovered = juce::jlimit (-1, static_cast<int> (state.bandsDb.size()) - 1, newHovered);
    }

    if (x >= geometry_.getPlotAreaLeft() && x <= geometry_.getPlotAreaLeft() + geometry_.getPlotAreaWidth() &&
        y >= geometry_.getPlotAreaTop() && y <= geometry_.getPlotAreaTop() + geometry_.getPlotAreaHeight())
    {
        const FreqAxisConfig freqConfig = buildFreqAxisConfig (state);
        freqHover_.setStyle (freqConfig.style);
        const float cursorXpx = x - geometry_.getPlotAreaLeft();
        if (freqHover_.updateFromCursorPx (cursorXpx, geometry_.getPlotAreaWidth(), freqConfig.mapping, freqConfig.ticks))
            needsRepaint = true;

        const DbAxisConfig dbConfig = buildDbAxisConfig (state);
        dbHover_.setStyle (dbConfig.style);
        const float cursorYpx = y - geometry_.getPlotAreaTop();
        if (dbHover_.updateFromCursorPx (cursorYpx, geometry_.getPlotAreaHeight(), dbConfig.mapping, dbConfig.ticks))
            needsRepaint = true;
    }
    else
    {
        if (freqHover_.deactivate() || dbHover_.deactivate())
            needsRepaint = true;
    }

    if (state.viewMode == 1)
    {
        if (state.logDb.empty())
        {
            hoveredBandIndex_ = -1;
            if (peakSnap_.deactivate())
                needsRepaint = true;
            requestRepaintMaybeRect (needsRepaint, state);
            return;
        }

        if (x >= geometry_.getPlotAreaLeft() && x <= geometry_.getPlotAreaLeft() + geometry_.getPlotAreaWidth() &&
            y >= geometry_.getPlotAreaTop() && y <= geometry_.getPlotAreaTop() + geometry_.getPlotAreaHeight())
        {
            const int numBands = static_cast<int> (state.logDb.size());
            static constexpr int kMaxBands = 4096;
            const int bandsToUse = std::min (numBands, kMaxBands);

            float freqHz[kMaxBands];
            float db[kMaxBands];

            const float logMin = std::log10 (state.minHz);
            const float logMax = std::log10 (state.maxHz);
            const float logRange = logMax - logMin;

            for (int i = 0; i < bandsToUse; ++i)
            {
                const float logPos = logMin + (static_cast<float> (i) + 0.5f) / static_cast<float> (numBands) * logRange;
                freqHz[i] = std::pow (10.0f, logPos);
                const auto idx = static_cast<size_t> (i);
                db[i] = (idx < state.logDb.size()) ? state.logDb[idx] : std::numeric_limits<float>::quiet_NaN();
            }

            mdsp_ui::AxisMapping freqMapping;
            freqMapping.scale = mdsp_ui::AxisScale::Log10;
            freqMapping.minValue = state.minHz;
            freqMapping.maxValue = state.maxHz;

            const juce::Rectangle<float> plotBoundsFloat (geometry_.getPlotAreaLeft(), geometry_.getPlotAreaTop(), geometry_.getPlotAreaWidth(), geometry_.getPlotAreaHeight());

            if (peakSnap_.updateFromCursorX (x, plotBoundsFloat, freqMapping, freqHz, db, bandsToUse))
                needsRepaint = true;
        }
        else
        {
            if (peakSnap_.deactivate())
                needsRepaint = true;
        }

        newHovered = geometry_.findNearestLogBand (x, state.logDb);
    }
    else if (state.viewMode == 0)
    {
        newHovered = -1;
        const bool inPlot = (x >= geometry_.getPlotAreaLeft() && x <= geometry_.getPlotAreaLeft() + geometry_.getPlotAreaWidth() &&
                            y >= geometry_.getPlotAreaTop() && y <= geometry_.getPlotAreaTop() + geometry_.getPlotAreaHeight());
        if (inPlot && state.sampleRate > 0.0 && state.fftSize > 0 && !state.fftDb.empty())
        {
            const int numBins = state.fftSize / 2 + 1;
            if (numBins > 0)
            {
                const float binHz = static_cast<float> (state.sampleRate) / static_cast<float> (state.fftSize);
                const float freq = geometry_.mapXToFreqFFT (x);
                const float clampedFreq = juce::jlimit (state.minHz, state.getEffectiveMaxHz(), freq);
                const int binIdx = geometry_.mapFreqToBinIndex (clampedFreq, state.sampleRate, state.fftSize);

                const float dbVal = getDbAtPixelX (x, state);
                const bool dbValid = state.hasValidSpectrumFrame && std::isfinite (dbVal) && dbVal > -200.0f;
                const float clampedDb = juce::jlimit (state.bottomDb, state.topDb + 18.0f, dbValid ? dbVal : state.bottomDb);
                const float snappedYpx = dbValid ? geometry_.dbToYPx (clampedDb + displayGainDb_) : (geometry_.getPlotAreaTop() + geometry_.getPlotAreaHeight() * 0.5f);
                const float snappedFreqHz = (binIdx >= 0) ? (static_cast<float> (binIdx) * binHz) : clampedFreq;

                fftHoverActive_ = true;
                fftHoverBinIndex_ = binIdx;
                fftHoverMouseXpx_ = x;
                fftHoverSnappedXpx_ = geometry_.freqHzToXPx (snappedFreqHz);
                fftHoverSnappedYpx_ = snappedYpx;
                fftHoverSnappedFreq_ = clampedFreq;
                fftHoverSnappedDb_ = dbVal;
                fftHoverDbValid_ = dbValid;
                if (dbValid)
                {
                    hoverDbTarget_ = clampedDb;
                    if (!hoverDbHasValue_)
                    {
                        hoverDbSmooth_ = hoverDbTarget_;
                        hoverDbHasValue_ = true;
                        hoverYSmoothPx_ = geometry_.dbToYPx (hoverDbSmooth_ + displayGainDb_);
                        hoverLastSmoothTimeSec_ = juce::Time::getMillisecondCounterHiRes() * 0.001;
                    }
                }
                else
                {
                    hoverDbHasValue_ = false;
                }

                if (clampedFreq >= 1000.0f)
                    fftHoverFreqText_ = juce::String (clampedFreq / 1000.0f, 2) + " kHz";
                else
                    fftHoverFreqText_ = juce::String (static_cast<int> (clampedFreq)) + " Hz";
                if (hoverDbHasValue_)
                    fftHoverReadoutText_ = fftHoverFreqText_ + "  " + juce::String (hoverDbTarget_, 1) + " dB";
                else
                    fftHoverReadoutText_ = fftHoverFreqText_ + "  \u2014";
                juce::GlyphArrangement ga;
                ga.addFittedText (juce::FontOptions().withHeight (10.0f), fftHoverReadoutText_,
                                  0.0f, 0.0f, 10000.0f, 10.0f, juce::Justification::left, 1);
                fftHoverReadoutWidth_ = ga.getBoundingBox (0, -1, true).getWidth();

                needsRepaint = true;
            }
            else
            {
                if (fftHoverActive_)
                    needsRepaint = true;
                fftHoverActive_ = false;
                hoverDbHasValue_ = false;
                hoverLastSmoothTimeSec_ = 0.0;
            }
        }
        else
        {
            if (fftHoverActive_)
                needsRepaint = true;
            fftHoverActive_ = false;
            hoverDbHasValue_ = false;
            hoverLastSmoothTimeSec_ = 0.0;
        }
    }
    if (newHovered != hoveredBandIndex_)
    {
        hoveredBandIndex_ = newHovered;
        needsRepaint = true;
    }

    requestRepaintMaybeRect (needsRepaint, state);
}

void RTADisplayController::onMouseExit (const juce::MouseEvent&)
{
    const auto& state = model_.getState();
    bool needsRepaint = false;
    if (hoveredBandIndex_ >= 0)
    {
        hoveredBandIndex_ = -1;
        needsRepaint = true;
    }
    if (fftHoverActive_)
    {
        fftHoverActive_ = false;
        hoverDbHasValue_ = false;
        hoverLastSmoothTimeSec_ = 0.0;
        needsRepaint = true;
    }
    if (freqHover_.deactivate() || dbHover_.deactivate())
        needsRepaint = true;
    if (peakSnap_.deactivate())
        needsRepaint = true;
    requestRepaintMaybeRect (needsRepaint, state);
}

void RTADisplayController::onMouseDown (const juce::MouseEvent& e)
{
    if (e.mods.isLeftButtonDown())
    {
        selectionActive_ = true;
        selectionRect_.setBounds (e.x, e.y, 0, 0);
#if JUCE_DEBUG
        if (e.mods.isShiftDown())
        {
            useEnvelopeDecimator_ = !useEnvelopeDecimator_;
            DBG ("RTADisplay: envelope decimator " << (useEnvelopeDecimator_ ? "ON" : "OFF"));
        }
#endif
        if (callbacks_.requestRepaint)
            callbacks_.requestRepaint();
    }
}

void RTADisplayController::onMouseDrag (const juce::MouseEvent& e)
{
    const auto& state = model_.getState();
    bool needsRepaint = false;

    if (state.viewMode == 0 && state.sampleRate > 0.0 && state.fftSize > 0 && !state.fftDb.empty())
    {
        const float x = static_cast<float> (e.x);
        const float y = static_cast<float> (e.y);
        const bool inPlot = (x >= geometry_.getPlotAreaLeft() && x <= geometry_.getPlotAreaLeft() + geometry_.getPlotAreaWidth() &&
                            y >= geometry_.getPlotAreaTop() && y <= geometry_.getPlotAreaTop() + geometry_.getPlotAreaHeight());
        if (inPlot)
        {
            const int numBins = state.fftSize / 2 + 1;
            if (numBins > 0)
            {
                const float binHz = static_cast<float> (state.sampleRate) / static_cast<float> (state.fftSize);
                const float freq = geometry_.mapXToFreqFFT (x);
                const float clampedFreq = juce::jlimit (state.minHz, state.getEffectiveMaxHz(), freq);
                const int binIdx = geometry_.mapFreqToBinIndex (clampedFreq, state.sampleRate, state.fftSize);

                const float dbVal = getDbAtPixelX (x, state);
                const bool dbValid = state.hasValidSpectrumFrame && std::isfinite (dbVal) && dbVal > -200.0f;
                const float clampedDb = juce::jlimit (state.bottomDb, state.topDb + 18.0f, dbValid ? dbVal : state.bottomDb);
                const float snappedYpx = dbValid ? geometry_.dbToYPx (clampedDb + displayGainDb_) : (geometry_.getPlotAreaTop() + geometry_.getPlotAreaHeight() * 0.5f);
                const float snappedFreqHz = (binIdx >= 0) ? (static_cast<float> (binIdx) * binHz) : clampedFreq;

                fftHoverActive_ = true;
                fftHoverBinIndex_ = binIdx;
                fftHoverMouseXpx_ = x;
                fftHoverSnappedXpx_ = geometry_.freqHzToXPx (snappedFreqHz);
                fftHoverSnappedYpx_ = snappedYpx;
                fftHoverSnappedFreq_ = clampedFreq;
                fftHoverSnappedDb_ = dbVal;
                fftHoverDbValid_ = dbValid;
                if (dbValid)
                {
                    hoverDbTarget_ = clampedDb;
                    if (!hoverDbHasValue_)
                    {
                        hoverDbSmooth_ = hoverDbTarget_;
                        hoverDbHasValue_ = true;
                        hoverYSmoothPx_ = geometry_.dbToYPx (hoverDbSmooth_ + displayGainDb_);
                        hoverLastSmoothTimeSec_ = juce::Time::getMillisecondCounterHiRes() * 0.001;
                    }
                }
                else
                {
                    hoverDbHasValue_ = false;
                }

                if (clampedFreq >= 1000.0f)
                    fftHoverFreqText_ = juce::String (clampedFreq / 1000.0f, 2) + " kHz";
                else
                    fftHoverFreqText_ = juce::String (static_cast<int> (clampedFreq)) + " Hz";
                if (hoverDbHasValue_)
                    fftHoverReadoutText_ = fftHoverFreqText_ + "  " + juce::String (hoverDbTarget_, 1) + " dB";
                else
                    fftHoverReadoutText_ = fftHoverFreqText_ + "  \u2014";
                juce::GlyphArrangement ga;
                ga.addFittedText (juce::FontOptions().withHeight (10.0f), fftHoverReadoutText_,
                                  0.0f, 0.0f, 10000.0f, 10.0f, juce::Justification::left, 1);
                fftHoverReadoutWidth_ = ga.getBoundingBox (0, -1, true).getWidth();

                needsRepaint = true;
            }
        }
        else
        {
            if (fftHoverActive_)
            {
                fftHoverActive_ = false;
                hoverDbHasValue_ = false;
                hoverLastSmoothTimeSec_ = 0.0;
                needsRepaint = true;
            }
        }
    }

    if (selectionActive_)
    {
        juce::Point<int> start = e.getMouseDownPosition();
        juce::Rectangle<int> newRect;
        newRect.setLeft (std::min (start.x, e.x));
        newRect.setTop (std::min (start.y, e.y));
        newRect.setWidth (std::abs (e.x - start.x));
        newRect.setHeight (std::abs (e.y - start.y));
        selectionRect_ = newRect;
        needsRepaint = true;
    }

    if (needsRepaint)
    {
        if (selectionActive_ && callbacks_.requestRepaint)
            callbacks_.requestRepaint();
        else
            requestRepaintMaybeRect (true, state);
    }
}

void RTADisplayController::onMouseUp (const juce::MouseEvent& e)
{
    if (selectionActive_)
    {
        if (e.mouseWasClicked() && !e.mods.isShiftDown())
        {
            selectionActive_ = false;
            selectionRect_ = {};
        }
        if (callbacks_.requestRepaint)
            callbacks_.requestRepaint();
    }
}

void RTADisplayController::onMouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&)
{
}
