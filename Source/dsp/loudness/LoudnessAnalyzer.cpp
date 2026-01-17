/*
  ==============================================================================

    LoudnessAnalyzer.cpp
    Created: 15 Jan 2026
    Author:  Antigravity

  ==============================================================================
*/

#include "LoudnessAnalyzer.h"

namespace AnalyzerPro::dsp
{

LoudnessAnalyzer::LoudnessAnalyzer()
{
    // Reserve logical size for history
    // Max capacity (e.g. 5 seconds of blocks at worst fragmentation)
    // 5 seconds * 1000 blocks/sec (small blocks) = 5000
    historyCapacity = 5000;
    historyBuffer.resize (static_cast<size_t> (historyCapacity));
}

void LoudnessAnalyzer::prepare (double sampleRate, int /*estimatedSamplesPerBlock*/)
{
    currentSampleRate = sampleRate;
    updateFilters();
    reset();
}

void LoudnessAnalyzer::reset()
{
    // Reset filters
    for (int ch = 0; ch < 2; ++ch)
    {
        if (preFilter[ch]) preFilter[ch]->reset();
        if (rlbFilter[ch]) rlbFilter[ch]->reset();
    }

    // Reset integration
    std::fill (historyBuffer.begin(), historyBuffer.end(), BlockEnergy{});
    historyWriteIndex = 0;
    
    integratedSumSquaresL = 1e-10;
    integratedSumSquaresR = 1e-10;
    integratedTotalSamples = 0;

    atomicM.store (-100.0f);
    atomicS.store (-100.0f);
    atomicI.store (-100.0f);
    atomicPeak.store (-100.0f);
}

void LoudnessAnalyzer::updateFilters()
{
    // K-weighting coefficients (ITU-R BS.1770-4)
    // Stage 1: High Shelf, +4dB at 1500Hz (roughly)
    // Stage 2: High Pass, ~38Hz
    
    // We use JUCE's IIR Coefficients
    // Note: Exact coefficients are defined in spec for 48kHz. 
    // JUCE's design functions approximate analog prototypes. 
    // This is "Reasonable" for V1.
    
    auto coefsStage1 = juce::dsp::IIR::Coefficients<float>::makeHighShelf (currentSampleRate, 1500.0f, 1.0f / std::sqrt(2.0f), juce::Decibels::decibelsToGain (4.0f));
    auto coefsStage2 = juce::dsp::IIR::Coefficients<float>::makeHighPass (currentSampleRate, 38.0f);

    for (int ch = 0; ch < 2; ++ch)
    {
        preFilter[ch] = std::make_unique<Filter> (coefsStage1);
        rlbFilter[ch] = std::make_unique<Filter> (coefsStage2);
    }
}

void LoudnessAnalyzer::process (const juce::AudioBuffer<float>& buffer)
{
    const int numSamples = buffer.getNumSamples();
    if (currentSampleRate <= 0.0 || numSamples == 0) return;

    // Work on a copy or process in place?
    // We should not modify the input buffer (const).
    // Allocate temp buffer on stack? 
    // numSamples can be large. Better use a member scratch buffer? 
    // Constraint: No allocations in processBlock. 
    // We can filter sample-by-sample or channel-by-channel with small stack array?
    // Or just assume max buffer size and have a member pre-allocated.
    // For V1, to match constraint "No allocations", let's process sample by sample accumulation.
    // That avoids large buffers.
    
    // We need 2 channels max.
    
    float sumSqL = 0.0f;
    float sumSqR = 0.0f;
    float peakDb = -100.0f;

    // Input channels
    const float* inL = buffer.getReadPointer (0);
    const float* inR = (buffer.getNumChannels() > 1) ? buffer.getReadPointer (1) : nullptr;

    for (int i = 0; i < numSamples; ++i)
    {
        // 1. Get input
        float l = inL[i];
        float r = (inR != nullptr) ? inR[i] : l;

        // Peak tracking (on raw signal?)
        // Standard usually tracks Peak/TruePeak of AUDIO, not weighted.
        // Let's track raw peak here.
        float absL = std::abs(l);
        float absR = std::abs(r);
        float maxAbs = std::max(absL, absR);
        if (maxAbs > 0.0f)
        {
             float  db = juce::Decibels::gainToDecibels(maxAbs);
             peakDb = std::max(peakDb, db);
        }

        // 2. K-Weighting
        // Stage 1
        l = preFilter[0]->processSample (l);
        r = preFilter[1]->processSample (r);

        // Stage 2
        l = rlbFilter[0]->processSample (l);
        r = rlbFilter[1]->processSample (r);

        // 3. Accumulate Power
        sumSqL += l * l;
        sumSqR += r * r;
    }

    // 4. Store Block Stats
    BlockEnergy& stats = historyBuffer[static_cast<size_t> (historyWriteIndex)];
    stats.sumSquaresL = sumSqL;
    stats.sumSquaresR = sumSqR;
    stats.numSamples = numSamples;

    // Advance ring buffer
    historyWriteIndex = (historyWriteIndex + 1) % historyCapacity;

    // 5. Update Integrated (Cumulative)
    integratedSumSquaresL += sumSqL;
    integratedSumSquaresR += sumSqR;
    integratedTotalSamples += numSamples;

    // 6. Compute Windows (Momentary 400ms, Short-term 3s)
    // Backward traversal from current write index
    
    auto computeWindow = [&](double durationSec) -> float
    {
        double requiredSamples = durationSec * currentSampleRate;
        double totalSqL = 0.0;
        double totalSqR = 0.0;
        int64_t accumulatedSamples = 0;
        

        // Wait, stats at (historyWriteIndex - 1) is the newest.
        
        // Scan backwards
        for (int k = 0; k < historyCapacity; ++k)
        {
            int readIdx = (historyWriteIndex - 1 - k);
            if (readIdx < 0) readIdx += historyCapacity;
            
            const auto& b = historyBuffer[static_cast<size_t> (readIdx)];
            if (b.numSamples == 0) break; // End of valid history (if not full yet)
            
            totalSqL += b.sumSquaresL;
            totalSqR += b.sumSquaresR;
            accumulatedSamples += b.numSamples;
            
            if (accumulatedSamples >= requiredSamples)
                break;
        }

        if (accumulatedSamples == 0) return -100.0f;
        
        // Mean Square
        double msL = totalSqL / (double)accumulatedSamples;
        double msR = totalSqR / (double)accumulatedSamples;
        
        // Loudness summation (ITU-R BS.1770)
        // meanSquare = mean(z_i^2)
        // No, for stereo: 
        // L = -0.691 + 10 log10( (1/T) sum(L^2) + (1/T) sum(R^2) )
        // Actually the formula handles summing channels:
        // L_k = -0.691 + 10 log10( mean_L + mean_R ) ?
        // Specification:
        // "Power summation" 
        // L_loudness = -0.691 + 10 * log10 ( z_L + z_R )
        // where z_L is mean square of Left, etc.
        
        return unitsToLufs(msL + msR);
    };

    float m = computeWindow (0.4);
    float s = computeWindow (3.0);
    
    // Integrated
    float i = -100.0f;
    if (integratedTotalSamples > 0)
    {
        double msL = integratedSumSquaresL / (double)integratedTotalSamples;
        double msR = integratedSumSquaresR / (double)integratedTotalSamples;
        i = unitsToLufs(msL + msR);
    }
    
    // Update atomics
    atomicM.store (m);
    atomicS.store (s);
    atomicI.store (i);
    
    // Peak hold logic could be here, but simpler to just push latest peak
    // UI will handle decay or hold?
    // Requirement "Peak/TruePeak". Just push latest block peak.
    // If block is small, this might be jittery.
    // Let's implement a simple decay here?
    // "No heavy recomputation".
    // Let's just store max(current, stored). 
    // Wait, atomicPeak is read by UI. 
    // If I just store 'peakDb', UI might miss it if polling is slow?
    // Better to use a hold or max.
    // Let's decay the atomic value?
    // Actually, UI runs at 10Hz (100ms).
    // If I just write peakDb, it might be overwritten by a quiet block before UI sees it.
    // So I need a "Peak Hold" for at least the UI polling interval.
    // Let's simple check:
    // Peak Hold (Max Hold, No Decay)
    float prevPeak = atomicPeak.load();
    if (peakDb > prevPeak)
    {
        atomicPeak.store(peakDb);
    }
}

void LoudnessAnalyzer::resetPeak()
{
    atomicPeak.store(-100.0f);
}

float LoudnessAnalyzer::unitsToLufs (double meanSquareSum) const
{
    if (meanSquareSum <= 1e-10) return -100.0f;
    return -0.691f + 10.0f * static_cast<float> (std::log10 (meanSquareSum));
}

LoudnessSnapshot LoudnessAnalyzer::getSnapshot() const
{
    LoudnessSnapshot s;
    s.momentaryLufs = atomicM.load();
    s.shortTermLufs = atomicS.load();
    s.integratedLufs = atomicI.load();
    s.peakDb = atomicPeak.load();
    return s;
}

} // namespace AnalyzerPro::dsp
