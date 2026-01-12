#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>
#include "parameters/Parameters.h"
#include "analyzer/AnalyzerEngine.h"
#include "hardware/HardwareMeterMapper.h"
#include "hardware/SoftwareMeterSink.h"
#include "state/PresetManager.h"
#include "state/StateManager.h"
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

    void setInputMeterMode (MeterMode mode) noexcept { inputMeterMode_.store (static_cast<int> (mode), std::memory_order_relaxed); }
    MeterMode getInputMeterMode() const noexcept { return static_cast<MeterMode> (inputMeterMode_.load (std::memory_order_relaxed)); }

    void setOutputMeterMode (MeterMode mode) noexcept { outputMeterMode_.store (static_cast<int> (mode), std::memory_order_relaxed); }
    MeterMode getOutputMeterMode() const noexcept { return static_cast<MeterMode> (outputMeterMode_.load (std::memory_order_relaxed)); }

    // Reset clears clip latch only (does not affect analyzer state/history).
    void resetMeterClipLatches() noexcept;
    const SoftwareMeterSink& getSoftwareMeterSink() const noexcept { return softwareMeterSink_; }

    void setEditorSize (int width, int height) noexcept { parameters.setEditorSize (width, height); }
    int getEditorWidth() const noexcept { return parameters.getEditorWidth(); }
    int getEditorHeight() const noexcept { return parameters.getEditorHeight(); }
    
    //==============================================================================
    AnalyzerPro::state::PresetManager& getPresetManager() { return *presetManager; }
    AnalyzerPro::state::StateManager& getStateManager() { return *stateManager; }
    
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
    // Cached raw APVTS parameter pointers (avoid map lookups in processBlock)
    std::atomic<float>* pFftSize_   = nullptr;
    std::atomic<float>* pAveraging_ = nullptr;
    std::atomic<float>* pPeakHold_ = nullptr;
    std::atomic<float>* pHold_      = nullptr;
    std::atomic<float>* pPeakDecay_ = nullptr;
    std::atomic<float>* pDbRange_   = nullptr;
    std::atomic<float>* pBypass_    = nullptr;
        
    // APVTS for analyzer controls
    juce::AudioProcessorValueTreeState apvts;
    
    // State Managers
    std::unique_ptr<AnalyzerPro::state::PresetManager> presetManager;
    std::unique_ptr<AnalyzerPro::state::StateManager> stateManager;

    MeterState inputMeters_[2];
    MeterState outputMeters_[2];
    
    std::atomic<int> inputMeterMode_ { 0 }; // 0=RMS
    std::atomic<int> outputMeterMode_ { 0 }; // 0=RMS

    float inputPeakEnv_[2] { 0.0f, 0.0f };
    float outputPeakEnv_[2]{ 0.0f, 0.0f };
    float inputRmsSq_[2]   { 0.0f, 0.0f };
    float outputRmsSq_[2]  { 0.0f, 0.0f };
    double meterSampleRate_ = 48000.0;

    HardwareMeterMapper hardwareMeterMapper_ { HardwareMeterMapper::Config { 16, false } };
    SoftwareMeterSink softwareMeterSink_;
    
    // Parameter creation helper
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();


    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AnalayzerProAudioProcessor)
};
