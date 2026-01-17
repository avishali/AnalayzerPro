#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>
#include "parameters/Parameters.h"
#include "analyzer/AnalyzerEngine.h"
#include "hardware/HardwareMeterMapper.h"
#include "hardware/SoftwareMeterSink.h"
#include "presets/PresetManager.h"
#include "presets/ABStateManager.h"
#include "dsp/loudness/LoudnessAnalyzer.h"
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
    struct MeterState
    {
        std::atomic<float> peakDb     { -120.0f };
        std::atomic<float> rmsDb      { -120.0f };
        std::atomic<bool>  clipLatched{ false };
    };

    enum class MeterMode
    {
        RMS = 0,
        Peak = 1
    };

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
    const MeterState* getInputMeterStates() const noexcept { return inputMeters_; }
    const MeterState* getOutputMeterStates() const noexcept { return outputMeters_; }
    int getMeterInputChannelCount() const noexcept;
    int getMeterOutputChannelCount() const noexcept;

    void setMeterMode (MeterMode mode) noexcept { meterMode_.store (static_cast<int> (mode), std::memory_order_relaxed); }
    MeterMode getMeterMode() const noexcept { return static_cast<MeterMode> (meterMode_.load (std::memory_order_relaxed)); }

    // Reset clears clip latch only (does not affect analyzer state/history).
    void resetMeterClipLatches() noexcept;
    const SoftwareMeterSink& getSoftwareMeterSink() const noexcept { return softwareMeterSink_; }

    void setEditorSize (int width, int height) noexcept { parameters.setEditorSize (width, height); }
    int getEditorWidth() const noexcept { return parameters.getEditorWidth(); }
    int getEditorHeight() const noexcept { return parameters.getEditorHeight(); }
    
    //==============================================================================
    AnalyzerPro::presets::PresetManager& getPresetManager() { return *presetManager; }
    AnalyzerPro::presets::ABStateManager& getABStateManager() { return *abStateManager; }

    AnalyzerPro::dsp::LoudnessAnalyzer& getLoudnessAnalyzer() { return loudnessAnalyzer; }
    
    //==============================================================================
    // Bypass Helpers
    bool getBypassState() const;
    void setBypassState (bool bypassed);

private:
    //==============================================================================
    Parameters parameters;
    AnalyzerEngine analyzerEngine;
    // Cached analyzer parameter values (to avoid calling setters every block)
    int   lastFftSizeIndex_ = -1;
    int   lastAveragingIndex_ = -1;
    bool  lastHold_ = false;
    float lastPeakDecayDbPerSec_ = std::numeric_limits<float>::quiet_NaN();
        
    // APVTS for analyzer controls
    juce::AudioProcessorValueTreeState apvts;
    
    // State Managers
    std::unique_ptr<AnalyzerPro::presets::PresetManager> presetManager;
    std::unique_ptr<AnalyzerPro::presets::ABStateManager> abStateManager;


    AnalyzerPro::dsp::LoudnessAnalyzer loudnessAnalyzer; // Integrated Loudness Analyzer

    MeterState inputMeters_[2];
    MeterState outputMeters_[2];
    
    std::atomic<int> meterMode_ { 0 }; // 0=RMS, 1=Peak (Shared)

    float inputPeakEnv_[2] { 0.0f, 0.0f };
    float outputPeakEnv_[2]{ 0.0f, 0.0f };
    float inputRmsSq_[2]   { 0.0f, 0.0f };
    float outputRmsSq_[2]  { 0.0f, 0.0f };
    double meterSampleRate_ = 48000.0;

    HardwareMeterMapper hardwareMeterMapper_ { HardwareMeterMapper::Config { 16, false } };
    SoftwareMeterSink softwareMeterSink_;
    
    // Scratch buffer for analysis (avoids modifying output buffer for visualization)
    juce::AudioBuffer<float> analysisBuffer;
    juce::AudioBuffer<float> outputAnalysisBuffer; // Added for V2 Output Metering
    
    // Cached parameter pointers
    std::atomic<float>* pFftSize_ = nullptr;
    std::atomic<float>* pAveraging_ = nullptr;
    std::atomic<float>* pHoldPeaks_ = nullptr;
    std::atomic<float>* pPeakDecay_ = nullptr;
    std::atomic<float>* pBypass_ = nullptr;
    
    // Trace Config Parameters
    std::atomic<float>* pTraceShowLR_ = nullptr;
    std::atomic<float>* pTraceShowMono_ = nullptr;
    std::atomic<float>* pTraceShowL_ = nullptr;
    std::atomic<float>* pTraceShowR_ = nullptr;
    std::atomic<float>* pTraceShowMid_ = nullptr;
    std::atomic<float>* pTraceShowSide_ = nullptr;
    std::atomic<float>* pTraceShowRMS_ = nullptr;
    
    // Parameter creation helper
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    
    // Migration helper
    // Migration helper
    static void migrateLegacyParameters (juce::ValueTree& state);

#if JucePlugin_Build_Standalone
    struct StandalonePersistence;
    std::unique_ptr<StandalonePersistence> standalonePersistence;
#endif


    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AnalayzerProAudioProcessor)
};
