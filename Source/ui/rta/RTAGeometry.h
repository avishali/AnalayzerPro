#pragma once

#include <juce_graphics/juce_graphics.h>
#include <vector>

//==============================================================================
/**
    Geometry and coordinate mapping for RTA display.
    Owns plot rectangles, mapping parameters, and provides coordinate conversion helpers.
    Does NOT paint (no juce::Graphics usage).
*/
class RTAGeometry
{
public:
    RTAGeometry();
    ~RTAGeometry() = default;

    // Band geometry structure
    struct BandGeometry
    {
        float xCenter = 0.0f;
        float xLeft = 0.0f;
        float xRight = 0.0f;
    };

    /** Update layout (plot area rectangles) from component bounds */
    void updateLayout (const juce::Rectangle<float>& bounds,
                       float leftMargin = 50.0f,
                       float rightMargin = 10.0f,
                       float topMargin = 10.0f,
                       float bottomMargin = 30.0f);

    /** Update configuration (frequency/dB ranges, sample rate, FFT size, etc.) */
    void updateConfig (float minHz, float maxHz,
                      float topDb, float bottomDb,
                      double sampleRate = 48000.0,
                      int fftSize = 2048);

    /** Update band centers for band geometry computation */
    void updateBandCenters (const std::vector<float>& bandCentersHz);

    // Plot area accessors
    float getPlotAreaLeft() const { return plotAreaLeft_; }
    float getPlotAreaTop() const { return plotAreaTop_; }
    float getPlotAreaWidth() const { return plotAreaWidth_; }
    float getPlotAreaHeight() const { return plotAreaHeight_; }
    juce::Rectangle<float> getPlotArea() const
    {
        return juce::Rectangle<float> (plotAreaLeft_, plotAreaTop_, plotAreaWidth_, plotAreaHeight_);
    }

    // Band geometry accessors
    bool isGeometryValid() const { return geometryValid_; }
    const std::vector<BandGeometry>& getBandGeometry() const { return bandGeometry_; }
    const std::vector<BandGeometry>& getLogGeometry() const { return logGeometry_; }

    // Coordinate mapping functions
    /** Convert frequency (Hz) to X pixel position */
    float freqHzToXPx (float freqHz) const;

    /** Convert X pixel position to frequency (Hz) */
    float xPxToFreqHz (float x) const;

    /** Convert dB to Y pixel position */
    float dbToYPx (float db) const;

    /** Convert Y pixel position to dB */
    float yPxToDb (float y) const;

    /** Find nearest band index from X position (for Bands mode) */
    int findNearestBand (float x) const;

    /** Find nearest log band index from X position (for Log mode) */
    int findNearestLogBand (float x, const std::vector<float>& logDb) const;

    /** Map X pixel to frequency for FFT mode */
    float mapXToFreqFFT (float x) const;

    /** Map frequency to FFT bin index */
    int mapFreqToBinIndex (float freqHz, double sampleRate, int fftSize) const;

    /** Compute log frequency from index */
    float computeLogFreqFromIndex (int index, int numBands, float minHz, float maxHz) const;

    /** Convert dB to Y with frequency-dependent compensation */
    float dbToYWithCompensation (float db, float freqHz, float tiltDb = 0.0f, float weightingDb = 0.0f) const;

private:
    // Plot area geometry
    float plotAreaLeft_ = 0.0f;
    float plotAreaTop_ = 0.0f;
    float plotAreaWidth_ = 0.0f;
    float plotAreaHeight_ = 0.0f;

    // Mapping parameters
    float minHz_ = 20.0f;
    float maxHz_ = 20000.0f;
    float topDb_ = 6.0f;
    float bottomDb_ = -200.0f;
    double sampleRate_ = 48000.0;
    int fftSize_ = 2048;

    // Cached log frequency mapping factors
    float logMinFreq_ = 0.0f;
    float logFreqRange_ = 0.0f;
    float dbRange_ = 0.0f;

    // Band geometry cache
    std::vector<BandGeometry> bandGeometry_;
    std::vector<BandGeometry> logGeometry_;
    bool geometryValid_ = false;

    // Internal helper: frequency to X using cached factors
    float frequencyToX (float freqHz) const;
};
