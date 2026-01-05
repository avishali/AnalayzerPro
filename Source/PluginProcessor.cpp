#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <atomic>
#include <cmath>

//==============================================================================
// TEMP ROUTING PROBE: remove after FFT confirmed.
// Audio input probe variables (atomic for thread-safe UI access)
namespace
{
    std::atomic<float> uiInputRmsDb  { -160.0f };
    std::atomic<float> uiInputPeakDb { -160.0f };
    std::atomic<int>   uiNumCh       { 0 };
}

//==============================================================================
AnalayzerProAudioProcessor::AnalayzerProAudioProcessor()
    : juce::AudioProcessor(
        #if JucePlugin_IsSynth
          BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)
        #else
          BusesProperties()
            .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
            .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
        #endif
      ),
      apvts (*this, nullptr, "PARAMETERS", createParameterLayout())
{
    // Add parameter listeners for analyzer controls
    apvts.addParameterListener ("Mode", this);
    apvts.addParameterListener ("FftSize", this);
    apvts.addParameterListener ("Averaging", this);
    apvts.addParameterListener ("Hold", this);
    apvts.addParameterListener ("PeakDecay", this);
}

AnalayzerProAudioProcessor::~AnalayzerProAudioProcessor()
{
}

//==============================================================================
const juce::String AnalayzerProAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool AnalayzerProAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool AnalayzerProAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool AnalayzerProAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double AnalayzerProAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int AnalayzerProAudioProcessor::getNumPrograms()
{
    return 1;
}

int AnalayzerProAudioProcessor::getCurrentProgram()
{
    return 0;
}

void AnalayzerProAudioProcessor::setCurrentProgram (int index)
{
    juce::ignoreUnused (index);
}

const juce::String AnalayzerProAudioProcessor::getProgramName (int index)
{
    juce::ignoreUnused (index);
    return {};
}

void AnalayzerProAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
    juce::ignoreUnused (index, newName);
}

//==============================================================================
void AnalayzerProAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
#if JUCE_DEBUG
    DBG ("Prepare: inCh=" << getTotalNumInputChannels() << " outCh=" << getTotalNumOutputChannels());
#endif
    analyzerEngine.prepare (sampleRate, samplesPerBlock);
}

void AnalayzerProAudioProcessor::releaseResources()
{
    analyzerEngine.reset();
}

bool AnalayzerProAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& mainOut = layouts.getMainOutputChannelSet();

    // Output must be mono or stereo
    if (mainOut != juce::AudioChannelSet::mono()
        && mainOut != juce::AudioChannelSet::stereo())
        return false;

    // Must have output enabled
    if (mainOut.isDisabled())
        return false;

#if !JucePlugin_IsSynth
    // For effect plugins: require input matches output
    const auto& mainIn = layouts.getMainInputChannelSet();
    
    // Input must be enabled for effects
    if (mainIn.isDisabled())
        return false;
    
    // Input must match output (pass-through analyzer requirement)
    if (mainIn != mainOut)
        return false;
#endif

    return true;
}

void AnalayzerProAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused (midiMessages);

    // Safe early-out for empty buffers
    if (buffer.getNumChannels() == 0 || buffer.getNumSamples() == 0)
        return;

    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();
    
#if JUCE_DEBUG
    if (totalNumInputChannels == 0)
        DBG ("processBlock: NO INPUT CHANNELS");
#endif

#if JUCE_DEBUG
    // TEMP DAW INPUT PROBE: verify non-zero samples from host (remove after verification)
    // NOTE: Ignore system log noise (HALC_ProxyIOContext, ViewBridge, CursorUI, Secure coding, etc.)
    static uint32_t dawProbeCounter = 0;
    if ((++dawProbeCounter % 60) == 0)  // ~1 time per second at 48kHz/512 samples
    {
        const int numCh = buffer.getNumChannels();
        const int n = buffer.getNumSamples();
        float maxAbsL = 0.0f;
        float maxAbsR = 0.0f;
        
        if (numCh > 0 && n > 0)
        {
            // Channel 0 (L)
            const float* ch0 = buffer.getReadPointer (0);
            for (int i = 0; i < n; ++i)
            {
                const float absVal = std::abs (ch0[i]);
                maxAbsL = juce::jmax (maxAbsL, absVal);
            }
            
            // Channel 1 (R) if present
            if (numCh > 1)
            {
                const float* ch1 = buffer.getReadPointer (1);
                for (int i = 0; i < n; ++i)
                {
                    const float absVal = std::abs (ch1[i]);
                    maxAbsR = juce::jmax (maxAbsR, absVal);
                }
            }
        }
        
        DBG ("DAW_INPUT: ch=" << numCh << " n=" << n << " maxAbs=[" << maxAbsL << "," << maxAbsR << "]");
    }
#endif
    
    // TEMP ROUTING PROBE: remove after FFT confirmed.
    // Measure input RMS and peak BEFORE any modifications (to verify host is sending audio)
    const int numCh = totalNumInputChannels;
    const int n = buffer.getNumSamples();
    float peak = 0.0f;
    float sumSquares = 0.0f;
    
    if (numCh > 0 && n > 0)
    {
        // Compute peak and RMS across all input channels
        for (int ch = 0; ch < numCh; ++ch)
        {
            const float* x = buffer.getReadPointer (ch);
            for (int i = 0; i < n; ++i)
            {
                const float absVal = std::abs (x[i]);
                peak = juce::jmax (peak, absVal);
                sumSquares += x[i] * x[i];
            }
        }
        
        // Compute RMS (average across all channels)
        const float rms = std::sqrt (sumSquares / static_cast<float> (n * juce::jmax (1, numCh)));
        
        // Convert to dB and store (relaxed order is sufficient for UI updates)
        uiNumCh.store (numCh, std::memory_order_relaxed);
        uiInputPeakDb.store (juce::Decibels::gainToDecibels (peak, -160.0f), std::memory_order_relaxed);
        uiInputRmsDb.store (juce::Decibels::gainToDecibels (rms, -160.0f), std::memory_order_relaxed);
        
        // TEMP ROUTING PROBE: throttled DBG print (once per 1000ms)
        static uint32_t lastPrintMs = 0;
        const auto nowMs = juce::Time::getMillisecondCounter();
        if (nowMs - lastPrintMs > 1000)
        {
            lastPrintMs = nowMs;
            DBG ("INPUT: ch=" << numCh
                 << " RMS=" << uiInputRmsDb.load (std::memory_order_relaxed)
                 << " PEAK=" << uiInputPeakDb.load (std::memory_order_relaxed));
        }
    }
    else
    {
        // No input channels: reset probe values
        uiNumCh.store (0, std::memory_order_relaxed);
        uiInputPeakDb.store (-160.0f, std::memory_order_relaxed);
        uiInputRmsDb.store (-160.0f, std::memory_order_relaxed);
    }

    // Clear any output channels that don't contain input data
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    // Apply gain
    const auto gainValue = parameters.getGain();
    if (gainValue != 1.0f)
    {
        for (int channel = 0; channel < totalNumInputChannels; ++channel)
            buffer.applyGain (channel, 0, buffer.getNumSamples(), gainValue);
    }
    
    // Update analyzer parameters from APVTS (audio thread, real-time safe)
    // Note: Mode is UI-only, handled on message thread
    auto* fftSizeParam = apvts.getRawParameterValue ("FftSize");
    auto* avgParam     = apvts.getRawParameterValue ("Averaging");
    auto* holdParam    = apvts.getRawParameterValue ("Hold");
    auto* decayParam   = apvts.getRawParameterValue ("PeakDecay");
    
    if (fftSizeParam != nullptr)
    {
        // Convert choice index to FFT size
        const int sizes[] = { 1024, 2048, 4096, 8192 };
        const int index = juce::roundToInt (fftSizeParam->load());
        if (index >= 0 && index < 4)
            analyzerEngine.setFftSize (sizes[index]);
    }
    
    if (avgParam != nullptr)
    {
        // Convert choice index to ms: Off=0, 50ms=1, 100ms=2, 250ms=3, 500ms=4, 1s=5
        const float avgMs[] = { 0.0f, 50.0f, 100.0f, 250.0f, 500.0f, 1000.0f };
        const int index = juce::roundToInt (avgParam->load());
        if (index >= 0 && index < 6)
            analyzerEngine.setAveragingMs (avgMs[index]);
    }
    
    if (holdParam != nullptr)
        analyzerEngine.setHold (holdParam->load() > 0.5f);
    
    if (decayParam != nullptr)
        analyzerEngine.setPeakDecayDbPerSec (decayParam->load());
    
    // IMPORTANT: AnalyzerEngine must be fed from the input signal (pre-mute, pre-gain, pre-output).
    // Feed analyzer (audio thread, real-time safe)
    analyzerEngine.processBlock (buffer);
}

//==============================================================================
bool AnalayzerProAudioProcessor::hasEditor() const
{
    return true;
}

juce::AudioProcessorEditor* AnalayzerProAudioProcessor::createEditor()
{
    return new AnalayzerProAudioProcessorEditor (*this);
}

//==============================================================================
void AnalayzerProAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    juce::ValueTree state ("PluginState");
    parameters.getState (state);
    juce::MemoryOutputStream mos (destData, true);
    state.writeToStream (mos);
}

void AnalayzerProAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto tree = juce::ValueTree::readFromData (data, static_cast<size_t> (sizeInBytes));
    if (tree.isValid())
        parameters.setState (tree);
}

//==============================================================================
void AnalayzerProAudioProcessor::parameterChanged (const juce::String& parameterID, float newValue)
{
    // Update analyzer engine parameters (called from message thread)
    // Note: Mode is handled in MainView to update AnalyzerDisplayView
    if (parameterID == "FftSize")
    {
        // Convert choice index to FFT size
        const int sizes[] = { 1024, 2048, 4096, 8192 };
        const int index = juce::roundToInt (newValue);
        if (index >= 0 && index < 4)
            analyzerEngine.setFftSize (sizes[index]);
    }
    else if (parameterID == "Averaging")
    {
        // Convert choice index to ms: Off=0, 50ms=1, 100ms=2, 250ms=3, 500ms=4, 1s=5
        const float avgMs[] = { 0.0f, 50.0f, 100.0f, 250.0f, 500.0f, 1000.0f };
        const int index = juce::roundToInt (newValue);
        if (index >= 0 && index < 6)
            analyzerEngine.setAveragingMs (avgMs[index]);
    }
    else if (parameterID == "Hold")
    {
        analyzerEngine.setHold (newValue > 0.5f);
    }
    else if (parameterID == "PeakDecay")
    {
        analyzerEngine.setPeakDecayDbPerSec (newValue);
    }
}

// ADD — Source/PluginProcessor.cpp (very bottom, after all code)
//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout AnalayzerProAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;
    
    // Analyzer Mode (choice: FFT=0, BANDS=1, LOG=2)
    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        "Mode", "Mode",
        juce::StringArray { "FFT", "BANDS", "LOG" },
        0,  // Default: FFT (index 0)
        "Mode"));
    
    // Analyzer FFT Size (choice: 1024, 2048, 4096, 8192)
    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        "FftSize", "FFT Size",
        juce::StringArray { "1024", "2048", "4096", "8192" },
        1,  // Default: 2048 (index 1)
        "FFT Size"));
    
    // Analyzer Averaging (choice: Off=0, 50ms=1, 100ms=2, 250ms=3, 500ms=4, 1s=5)
    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        "Averaging", "Averaging",
        juce::StringArray { "Off", "50 ms", "100 ms", "250 ms", "500 ms", "1 s" },
        2,  // Default: 100ms (index 2)
        "Averaging"));
    
    // Analyzer Hold (bool, default false)
    params.push_back (std::make_unique<juce::AudioParameterBool> (
        "Hold", "Hold",
        false,  // Default: off
        "Hold"));
    
    // Analyzer Peak Decay (0..10 dB/sec, default 1.0 dB/sec)
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "PeakDecay", "Peak Decay",
        juce::NormalisableRange<float> (0.0f, 10.0f, 0.1f),
        1.0f,  // Default: 1.0 dB/sec
        "Peak Decay (dB/s)"));
    
    return { params.begin(), params.end() };
}

//==============================================================================
// TEMP ROUTING PROBE: remove after FFT confirmed.
float AnalayzerProAudioProcessor::getUiInputRmsDb() const noexcept
{
    return uiInputRmsDb.load (std::memory_order_relaxed);
}

float AnalayzerProAudioProcessor::getUiInputPeakDb() const noexcept
{
    return uiInputPeakDb.load (std::memory_order_relaxed);
}

int AnalayzerProAudioProcessor::getUiNumCh() const noexcept
{
    return uiNumCh.load (std::memory_order_relaxed);
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AnalayzerProAudioProcessor();
}
