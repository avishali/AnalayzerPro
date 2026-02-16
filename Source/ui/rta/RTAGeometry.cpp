#include "RTAGeometry.h"
#include <limits>

//==============================================================================
RTAGeometry::RTAGeometry()
{
    // Initialize with defaults
    minHz_ = 20.0f;
    maxHz_ = 20000.0f;
    topDb_ = 6.0f;
    bottomDb_ = -200.0f;
    logMinFreq_ = std::log10 (minHz_);
    logFreqRange_ = std::log10 (maxHz_) - logMinFreq_;
    dbRange_ = topDb_ - bottomDb_;
}

//==============================================================================
void RTAGeometry::updateLayout (const juce::Rectangle<float>& bounds,
                                 float leftMargin,
                                 float rightMargin,
                                 float topMargin,
                                 float bottomMargin)
{
    plotAreaLeft_ = leftMargin;
    plotAreaTop_ = topMargin;
    plotAreaWidth_ = bounds.getWidth() - leftMargin - rightMargin;
    plotAreaHeight_ = bounds.getHeight() - topMargin - bottomMargin;

    // Guardrails - if bounds too small, clear geometry
    if (plotAreaWidth_ <= 1.0f || plotAreaHeight_ <= 1.0f)
    {
        bandGeometry_.clear();
        logGeometry_.clear();
        geometryValid_ = false;
        return;
    }

    geometryValid_ = true;
}

//==============================================================================
void RTAGeometry::updateConfig (float minHz, float maxHz,
                                float topDb, float bottomDb,
                                double sampleRate,
                                int fftSize)
{
    minHz_ = minHz;
    maxHz_ = maxHz;
    topDb_ = topDb;
    bottomDb_ = bottomDb;
    sampleRate_ = sampleRate;
    fftSize_ = fftSize;

    // Update cached log frequency mapping factors
    logMinFreq_ = std::log10 (minHz_);
    logFreqRange_ = std::log10 (maxHz_) - logMinFreq_;
    dbRange_ = topDb_ - bottomDb_;
}

//==============================================================================
void RTAGeometry::updateBandCenters (const std::vector<float>& bandCentersHz)
{
    if (bandCentersHz.empty())
    {
        bandGeometry_.clear();
        geometryValid_ = true;  // Valid but empty
        return;
    }

    if (!geometryValid_ || plotAreaWidth_ <= 1.0f || plotAreaHeight_ <= 1.0f)
    {
        bandGeometry_.clear();
        return;
    }

    bandGeometry_.resize (bandCentersHz.size());

    for (size_t i = 0; i < bandCentersHz.size(); ++i)
    {
        const float centerFreq = bandCentersHz[i];
        float xCenter = frequencyToX (centerFreq);

        // Compute band width from adjacent bands or use fixed ratio
        float xLeft, xRight;
        if (i == 0)
        {
            // First band: use next band or fixed width
            if (bandCentersHz.size() > 1)
            {
                const float nextCenter = frequencyToX (bandCentersHz[static_cast<size_t> (1)]);
                const float width = (nextCenter - xCenter) * 0.5f;
                xLeft = xCenter - width;
                xRight = xCenter + width;
            }
            else
            {
                xLeft = xCenter - 5.0f;
                xRight = xCenter + 5.0f;
            }
        }
        else if (i == bandCentersHz.size() - 1)
        {
            // Last band: use previous band
            const float prevCenter = frequencyToX (bandCentersHz[i - 1]);
            const float width = (xCenter - prevCenter) * 0.5f;
            xLeft = xCenter - width;
            xRight = xCenter + width;
        }
        else
        {
            // Middle bands: use adjacent centers
            const float prevCenter = frequencyToX (bandCentersHz[i - 1]);
            const float nextCenter = frequencyToX (bandCentersHz[i + 1]);
            xLeft = (prevCenter + xCenter) * 0.5f;
            xRight = (xCenter + nextCenter) * 0.5f;
        }

        // Clamp to plot area
        xLeft = juce::jmax (plotAreaLeft_, xLeft);
        xRight = juce::jmin (plotAreaLeft_ + plotAreaWidth_, xRight);

        bandGeometry_[i].xCenter = xCenter;
        bandGeometry_[i].xLeft = xLeft;
        bandGeometry_[i].xRight = xRight;
    }
}

//==============================================================================
float RTAGeometry::frequencyToX (float freqHz) const
{
    // Guardrails - handle invalid log ranges
    if (freqHz <= 0.0f || logFreqRange_ <= 0.0f)
        return plotAreaLeft_;

    const float logFreq = std::log10 (freqHz);
    const float normalized = (logFreq - logMinFreq_) / logFreqRange_;
    return plotAreaLeft_ + normalized * plotAreaWidth_;
}

//==============================================================================
float RTAGeometry::freqHzToXPx (float freqHz) const
{
    // Guardrails - handle invalid log ranges
    if (freqHz <= 0.0f || maxHz_ <= minHz_ || minHz_ <= 0.0f)
        return plotAreaLeft_;

    const float logMin = std::log10 (minHz_);
    const float logMax = std::log10 (maxHz_);
    const float logRange = logMax - logMin;
    if (logRange <= 0.0f)
        return plotAreaLeft_;

    const float logFreq = std::log10 (freqHz);
    const float normalized = (logFreq - logMin) / logRange;
    return plotAreaLeft_ + normalized * plotAreaWidth_;
}

//==============================================================================
float RTAGeometry::xPxToFreqHz (float x) const
{
    if (plotAreaWidth_ <= 0.0f || maxHz_ <= minHz_ || minHz_ <= 0.0f)
        return minHz_;

    const float norm = (x - plotAreaLeft_) / plotAreaWidth_;
    const float logMin = std::log10 (minHz_);
    const float logMax = std::log10 (maxHz_);
    const float logRange = logMax - logMin;
    const float logFreq = logMin + norm * logRange;
    return std::pow (10.0f, juce::jlimit (logMin, logMax, logFreq));
}

//==============================================================================
float RTAGeometry::dbToYPx (float db) const
{
    // Clamp to display range
    const float clampedDb = juce::jlimit (bottomDb_, topDb_, db);

    if (dbRange_ <= 0.0f)
        return plotAreaTop_;
    const float normalized = (topDb_ - clampedDb) / dbRange_;
    return plotAreaTop_ + normalized * plotAreaHeight_;
}

//==============================================================================
float RTAGeometry::yPxToDb (float y) const
{
    if (plotAreaHeight_ <= 0.0f || dbRange_ <= 0.0f)
        return bottomDb_;

    const float normalized = (y - plotAreaTop_) / plotAreaHeight_;
    const float db = topDb_ - normalized * dbRange_;
    return juce::jlimit (bottomDb_, topDb_, db);
}

//==============================================================================
int RTAGeometry::findNearestBand (float x) const
{
    // Only used for Bands mode - uses bandGeometry
    if (!geometryValid_ || bandGeometry_.empty())
        return -1;

    int nearest = -1;
    float minDist = std::numeric_limits<float>::max();

    for (size_t i = 0; i < bandGeometry_.size(); ++i)
    {
        const float dist = std::abs (x - bandGeometry_[i].xCenter);
        if (dist < minDist)
        {
            minDist = dist;
            nearest = static_cast<int> (i);
        }
    }

    return nearest;
}

//==============================================================================
int RTAGeometry::findNearestLogBand (float x, const std::vector<float>& logDb) const
{
    // Find nearest log band index from x position
    if (x < plotAreaLeft_ || x > plotAreaLeft_ + plotAreaWidth_ || logDb.empty())
        return -1;

    const float logMin = std::log10 (minHz_);
    const float logMax = std::log10 (maxHz_);
    const float logRange = logMax - logMin;
    if (logRange <= 0.0f)
        return -1;

    // Inverse mapping: x -> normalized -> log position -> index
    const float normalized = (x - plotAreaLeft_) / plotAreaWidth_;
    const float logPos = logMin + normalized * logRange;

    // Map log position directly to index (no need to compute freq)
    const float normFromFreq = (logPos - logMin) / logRange;
    const int numBands = static_cast<int> (logDb.size());
    const int idx = juce::jlimit (0, numBands - 1, static_cast<int> (normFromFreq * numBands));

    return idx;
}

//==============================================================================
float RTAGeometry::mapXToFreqFFT (float x) const
{
    if (plotAreaWidth_ <= 0.0f || maxHz_ <= minHz_ || minHz_ <= 0.0f)
        return minHz_;

    const float norm = (x - plotAreaLeft_) / plotAreaWidth_;
    const float logMin = std::log10 (minHz_);
    const float logMax = std::log10 (maxHz_);
    const float logRange = logMax - logMin;
    const float logFreq = logMin + norm * logRange;
    return std::pow (10.0f, juce::jlimit (logMin, logMax, logFreq));
}

//==============================================================================
int RTAGeometry::mapFreqToBinIndex (float freqHz, double sampleRate, int fftSize) const
{
    if (sampleRate <= 0.0 || fftSize <= 0)
        return -1;

    const float binHz = static_cast<float> (sampleRate) / static_cast<float> (fftSize);
    if (binHz <= 0.0f)
        return -1;

    const int numBins = fftSize / 2 + 1;
    const int idx = static_cast<int> (std::round (freqHz / binHz));
    return juce::jlimit (0, numBins - 1, idx);
}

//==============================================================================
float RTAGeometry::computeLogFreqFromIndex (int index, int numBands, float minHz, float maxHz) const
{
    // Compute log frequency from index (for log mode rendering)
    if (numBands <= 0 || index < 0 || index >= numBands)
        return minHz;

    const float logMin = std::log10 (minHz);
    const float logMax = std::log10 (maxHz);
    const float logRange = logMax - logMin;
    const float logPos = logMin + (static_cast<float> (index) + 0.5f) / static_cast<float> (numBands) * logRange;
    return std::pow (10.0f, logPos);
}

//==============================================================================
float RTAGeometry::dbToYWithCompensation (float db, float freqHz, float tiltDb, float weightingDb) const
{
    (void) freqHz; // Used when tilt/weighting are re-enabled

    const float dbWithCompensation = db + tiltDb + weightingDb;

    // Clamp to display range (allow +18dB headroom above topDb for peaks/overs)
    const float clampedDb = juce::jlimit (bottomDb_, topDb_ + 18.0f, dbWithCompensation);

    if (dbRange_ <= 0.0f)
        return plotAreaTop_;

    const float normalized = (topDb_ - clampedDb) / dbRange_;
    return plotAreaTop_ + normalized * plotAreaHeight_;
}
