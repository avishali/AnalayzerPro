#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>
#include "parameters/Parameters.h"
#include "analyzer/AnalyzerEngine.h"
#include <limits>


//==============================================================================
/**
    Audio Processor for AnalyzerPro plugin.
    Effect plugin with stereo input/output buses (or mono if selected).
    Analyzes audio input and displays FFT/BANDS/LOG spectrum.
*/
class AnalayzerProAudioProcessor : public juce::AudioProcessor,
                                    public juce::AudioProcessorValueTreeState::Listener
{
public:
    //==============================================================================
    AnalayzerProAudioProcessor();
    ~AnalayzerProAudioProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    //==============================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;
    
    //==============================================================================
    void parameterChanged (const juce::String& parameterID, float newValue) override;

    //==============================================================================
    Parameters& getParameters() { return parameters; }
    const Parameters& getParameters() const { return parameters; }

    //==============================================================================
    AnalyzerEngine& getAnalyzerEngine() noexcept { return analyzerEngine; }
    const AnalyzerEngine& getAnalyzerEngine() const noexcept { return analyzerEngine; }

    //==============================================================================
    juce::AudioProcessorValueTreeState& getAPVTS() noexcept { return apvts; }
    const juce::AudioProcessorValueTreeState& getAPVTS() const noexcept { return apvts; }
    
    //==============================================================================
    // TEMP ROUTING PROBE: remove after FFT confirmed.
    /** Get input RMS level in dB (for debug overlay) */
    float getUiInputRmsDb() const noexcept;
    /** Get input peak level in dB (for debug overlay) */
    float getUiInputPeakDb() const noexcept;
    /** Get number of input channels (for debug overlay) */
    int getUiNumCh() const noexcept;

private:
    //==============================================================================
    Parameters parameters;
    AnalyzerEngine analyzerEngine;
    // Cached analyzer parameter values (to avoid calling setters every block)
    int   lastFftSizeIndex_ = -1;
    int   lastAveragingIndex_ = -1;
    bool  lastHold_ = false;
    float lastPeakDecayDbPerSec_ = std::numeric_limits<float>::quiet_NaN();
    // Cached raw APVTS parameter pointers (avoid map lookups in processBlock)
    std::atomic<float>* pFftSize_   = nullptr;
    std::atomic<float>* pAveraging_ = nullptr;
    std::atomic<float>* pHold_      = nullptr;
    std::atomic<float>* pPeakDecay_ = nullptr;
        
    // APVTS for analyzer controls
    juce::AudioProcessorValueTreeState apvts;
    
    // Parameter creation helper
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();


    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AnalayzerProAudioProcessor)
};
