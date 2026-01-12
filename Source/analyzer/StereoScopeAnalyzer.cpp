#include "StereoScopeAnalyzer.h"

StereoScopeAnalyzer::StereoScopeAnalyzer()
{
    bufferLeft_.resize (kBufferSize, 0.0f);
    bufferRight_.resize (kBufferSize, 0.0f);
}

StereoScopeAnalyzer::~StereoScopeAnalyzer() = default;

void StereoScopeAnalyzer::pushSamples (const float* left, const float* right, int numSamples)
{
    // Audio thread: simply push to FIFO
    // juce::AbstractFifo::write handles wrapping logic
    
    int start1, size1, start2, size2;
    fifo_.prepareToWrite (numSamples, start1, size1, start2, size2);

    if (size1 > 0)
    {
        // Copy first chunk
        juce::FloatVectorOperations::copy (bufferLeft_.data() + start1, left, size1);
        juce::FloatVectorOperations::copy (bufferRight_.data() + start1, right, size1);
    }

    if (size2 > 0)
    {
        // Copy wrapped chunk
        juce::FloatVectorOperations::copy (bufferLeft_.data() + start2, left + size1, size2);
        juce::FloatVectorOperations::copy (bufferRight_.data() + start2, right + size1, size2);
    }

    fifo_.finishedWrite (size1 + size2);
}

int StereoScopeAnalyzer::getSnapshot (std::vector<float>& destLeft, std::vector<float>& destRight, int numSamplesToRead)
{
    // UI Thread: read latest samples
    // If FIFO has fewer samples than requested, we read available.
    // If FIFO has MORE, we read the LATEST (skip old ones).
    
    // Logic: we want the *latest* 'numSamplesToRead' for a realtime scope.
    // If fifo has 1000 samples and we want 512, we should read the LAST 512 written?
    // Or just read whatever is next?
    // A scope usually consumes contiguous audio.
    // But if UI is slow (30fps), we miss data.
    // "Vectorscope" usually shows "a window of time".
    // Skipping data is fine, but tearing is bad.
    // AbstractFifo reads from read pointer.
    // Typically for a scope, we just drain the buffer and display the *last* chunk?
    // Or we display *all* chunks since last frame?
    // To minimize UI load, let's just grab the *most recent* chunk of size N.
    // But AbstractFifo points to "oldest unread".
    
    // Strategy: Read *all* available, but only keep the last N?
    // Optimization: check 'getNumReady()'. If > N, skip (advance read pointer).
    
    const int ready = fifo_.getNumReady();
    if (ready == 0)
        return 0;

    int validSamples = numSamplesToRead;
    
    // If we have TOO data (lag), jump ahead
    if (ready > numSamplesToRead)
    {
        const int toSkip = ready - numSamplesToRead;
        // Advance read pointer without reading
        int s1, sz1, s2, sz2;
        fifo_.prepareToRead (toSkip, s1, sz1, s2, sz2);
        fifo_.finishedRead (sz1 + sz2);
    }
    else
    {
        validSamples = ready;
    }
    
    // Now read the valid chunk
    int start1, size1, start2, size2;
    fifo_.prepareToRead (validSamples, start1, size1, start2, size2);
    
    // Ensure dest buffers are big enough
    if (destLeft.size() < static_cast<size_t> (validSamples)) destLeft.resize (static_cast<size_t> (validSamples));
    if (destRight.size() < static_cast<size_t> (validSamples)) destRight.resize (static_cast<size_t> (validSamples));
    
    if (size1 > 0)
    {
        juce::FloatVectorOperations::copy (destLeft.data(), bufferLeft_.data() + start1, size1);
        juce::FloatVectorOperations::copy (destRight.data(), bufferRight_.data() + start1, size1);
    }
    
    if (size2 > 0)
    {
        juce::FloatVectorOperations::copy (destLeft.data() + size1, bufferLeft_.data() + start2, size2);
        juce::FloatVectorOperations::copy (destRight.data() + size1, bufferRight_.data() + start2, size2);
    }
    
    fifo_.finishedRead (size1 + size2);
    
    return validSamples;
}
