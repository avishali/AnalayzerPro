#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <atomic>
#include <cmath>
#include <limits>
#include <iterator>

#ifndef JUCE_UNLIKELY
// JUCE does not always provide JUCE_UNLIKELY as a macro in all module include paths.
// Define a local, compiler-friendly fallback (RT-safe, branch-predictable).
 #if defined(__clang__) || defined(__GNUC__)
  #define JUCE_UNLIKELY(x) __builtin_expect (!!(x), 0)
 #else
  #define JUCE_UNLIKELY(x) (x)
 #endif
#endif

#if JucePlugin_Build_Standalone
#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_audio_plugin_client/Standalone/juce_StandaloneFilterWindow.h>
#include "audio/DeviceRoutingHelper.h"
#endif

//==============================================================================
namespace
{
    inline void applyPeakHoldFromDecayParam (AnalyzerEngine& engine, float decayDbPerSec)
    {
        // PeakDecay maps directly to engine peak decay (no additional modes/settings here).
        engine.setPeakDecayDbPerSec (decayDbPerSec);
    }
}
namespace
{
    static inline float linToDb (float lin) noexcept
    {
        constexpr float kEps = 1.0e-9f;
        const float v = (lin > kEps) ? lin : kEps;
        return 20.0f * std::log10 (v);
    }

    static inline float clampStoredDb (float db) noexcept
    {
        return std::isfinite (db) ? juce::jmax (db, -120.0f) : -120.0f;
    }
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
    // Analyzer parameters are polled and applied in processBlock (single source of truth).
    // Avoid APVTS listeners here to prevent double-application and to keep RT behavior predictable.
    // Cache raw parameter pointers once (valid for lifetime of APVTS)
    pFftSize_   = apvts.getRawParameterValue ("FftSize");
    pAveraging_ = apvts.getRawParameterValue ("Averaging");
    pPeakHold_  = apvts.getRawParameterValue ("PeakHold");
    pHold_      = apvts.getRawParameterValue ("Hold");
    pPeakDecay_ = apvts.getRawParameterValue ("PeakDecay");
    pDbRange_   = apvts.getRawParameterValue ("DbRange");

    #if JUCE_DEBUG
    jassert (pFftSize_   != nullptr);
    jassert (pAveraging_ != nullptr);
    jassert (pPeakHold_  != nullptr);
    jassert (pHold_      != nullptr);
    jassert (pPeakDecay_ != nullptr);
    jassert (pDbRange_   != nullptr);
    #endif
#if JucePlugin_Build_Standalone
    // Auto-configure macOS loopback routing for standalone builds
    // Defer the routing setup to ensure StandalonePluginHolder is initialized
    juce::Timer::callAfterDelay(100, []
    {
        if (auto* holder = juce::StandalonePluginHolder::getInstance())
        {
            auto& dm = holder->deviceManager;
            auto r = analyzerpro::DeviceRoutingHelper::applyMacSystemCapture(dm, "AnalyzerPro Aggregate", 48000.0, 256);
            DBG("[AnalyzerPro] " + r.message);
        }
        else
        {
            DBG("[AnalyzerPro] StandalonePluginHolder instance not available.");
        }
    });
#endif
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

    meterSampleRate_ = (sampleRate > 1.0 ? sampleRate : 48000.0);
    for (int ch = 0; ch < 2; ++ch)
    {
        inputPeakEnv_[ch] = 0.0f;
        outputPeakEnv_[ch] = 0.0f;
        inputRmsSq_[ch] = 0.0f;
        outputRmsSq_[ch] = 0.0f;

        inputMeters_[ch].peakDb.store (-120.0f, std::memory_order_relaxed);
        inputMeters_[ch].rmsDb.store (-120.0f, std::memory_order_relaxed);
        inputMeters_[ch].clipLatched.store (false, std::memory_order_relaxed);

        outputMeters_[ch].peakDb.store (-120.0f, std::memory_order_relaxed);
        outputMeters_[ch].rmsDb.store (-120.0f, std::memory_order_relaxed);
        outputMeters_[ch].clipLatched.store (false, std::memory_order_relaxed);
    }

    lastFftSizeIndex_ = -1;
    lastAveragingIndex_ = -1;
    lastHold_ = false;
    lastPeakDecayDbPerSec_ = std::numeric_limits<float>::quiet_NaN();
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

    if (JUCE_UNLIKELY (pFftSize_ == nullptr ||
                       pAveraging_ == nullptr ||
                       pPeakHold_ == nullptr ||
                       pHold_ == nullptr ||
                       pPeakDecay_ == nullptr))
    {
        return; // Safety guard (should never happen)
    }

    // Safe early-out for empty buffers
    if (buffer.getNumChannels() == 0 || buffer.getNumSamples() == 0)
        return;

    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();
    
    const int n = buffer.getNumSamples();
    const float dtSec = static_cast<float> (n / juce::jmax (1.0, meterSampleRate_));

    // Input meters: measure buffer pre-processing.
    const int inChCount = juce::jlimit (0, 2, totalNumInputChannels);
    for (int ch = 0; ch < 2; ++ch)
    {
        if (ch >= inChCount)
        {
            inputMeters_[ch].peakDb.store (-120.0f, std::memory_order_relaxed);
            inputMeters_[ch].rmsDb.store (-120.0f, std::memory_order_relaxed);
            continue;
        }

        const float* x = buffer.getReadPointer (ch);
        float blockPeak = 0.0f;
        float sumSq = 0.0f;
        bool clipped = false;

        for (int i = 0; i < n; ++i)
        {
            const float s = x[i];
            const float a = std::abs (s);
            blockPeak = (a > blockPeak) ? a : blockPeak;
            sumSq += s * s;
            clipped = clipped || (a >= 1.0f);
        }

        const float blockMeanSq = sumSq / static_cast<float> (n);

        // Peak: instantaneous attack, ~300ms exponential release.
        const float peakReleaseCoeff = std::exp (-dtSec / 0.30f);
        inputPeakEnv_[ch] = juce::jmax (blockPeak, inputPeakEnv_[ch] * peakReleaseCoeff);

        // RMS: EMA of squared signal (~300ms attack, ~400ms release).
        const float tau = (blockMeanSq > inputRmsSq_[ch]) ? 0.30f : 0.40f;
        const float rmsCoeff = std::exp (-dtSec / tau);
        inputRmsSq_[ch] = (rmsCoeff * inputRmsSq_[ch]) + ((1.0f - rmsCoeff) * blockMeanSq);

        const float peakDb = clampStoredDb (linToDb (inputPeakEnv_[ch]));
        float rmsDb = clampStoredDb (linToDb (std::sqrt (inputRmsSq_[ch])));
        rmsDb = juce::jmin (rmsDb, peakDb); // RMS must never exceed peak visually.

        inputMeters_[ch].peakDb.store (peakDb, std::memory_order_relaxed);
        inputMeters_[ch].rmsDb.store (rmsDb, std::memory_order_relaxed);
        if (clipped)
            inputMeters_[ch].clipLatched.store (true, std::memory_order_relaxed);
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
        constexpr int sizes[] = { 1024, 2048, 4096, 8192 };
        constexpr int kNumSizes = static_cast<int> (std::size (sizes));

        const float raw = fftSizeParam->load();
        const int index = juce::jlimit (0, kNumSizes - 1, static_cast<int> (raw));

       #if JUCE_DEBUG
        // Choice params should be discrete indices; if hosts send fractional values, clamp will still behave.
        jassert (std::abs (raw - static_cast<float> (index)) < 0.001f);
       #endif

        if (index != lastFftSizeIndex_)
        {
            lastFftSizeIndex_ = index;
            analyzerEngine.requestFftSize (sizes[index]);
        }
    }

    if (avgParam != nullptr)
    {
        constexpr float avgMs[] = { 0.0f, 50.0f, 100.0f, 250.0f, 500.0f, 1000.0f };
        constexpr int kNumAvg = static_cast<int> (std::size (avgMs));

        const float raw = avgParam->load();
        const int index = juce::jlimit (0, kNumAvg - 1, static_cast<int> (raw));

       #if JUCE_DEBUG
        jassert (std::abs (raw - static_cast<float> (index)) < 0.001f);
       #endif

        if (index != lastAveragingIndex_)
        {
            lastAveragingIndex_ = index;
            analyzerEngine.setAveragingMs (avgMs[index]);
        }
    }

    if (holdParam != nullptr)
    {
        const bool hold = (holdParam->load() > 0.5f);
        if (hold != lastHold_)
        {
            lastHold_ = hold;
            analyzerEngine.setHold (hold);
        }
    }

    if (decayParam != nullptr)
    {
        const float decay = decayParam->load();

        const bool first = std::isnan (lastPeakDecayDbPerSec_);
        const bool changed = first || (std::abs (decay - lastPeakDecayDbPerSec_) > 1.0e-3f);

        if (changed)
        {
            lastPeakDecayDbPerSec_ = decay;
            applyPeakHoldFromDecayParam (analyzerEngine, decay);
        }
    }
    // IMPORTANT: AnalyzerEngine must be fed from the input signal (pre-mute, pre-gain, pre-output).
    // Feed analyzer (audio thread, real-time safe)
    analyzerEngine.processBlock (buffer);

    // Output meters: measure buffer post-processing (after gain / processing).
    const int outChCount = juce::jlimit (0, 2, totalNumOutputChannels);
    for (int ch = 0; ch < 2; ++ch)
    {
        if (ch >= outChCount)
        {
            outputMeters_[ch].peakDb.store (-120.0f, std::memory_order_relaxed);
            outputMeters_[ch].rmsDb.store (-120.0f, std::memory_order_relaxed);
            continue;
        }

        const float* y = buffer.getReadPointer (ch);
        float blockPeak = 0.0f;
        float sumSq = 0.0f;
        bool clipped = false;

        for (int i = 0; i < n; ++i)
        {
            const float s = y[i];
            const float a = std::abs (s);
            blockPeak = (a > blockPeak) ? a : blockPeak;
            sumSq += s * s;
            clipped = clipped || (a >= 1.0f);
        }

        const float blockMeanSq = sumSq / static_cast<float> (n);

        const float peakReleaseCoeff = std::exp (-dtSec / 0.30f);
        outputPeakEnv_[ch] = juce::jmax (blockPeak, outputPeakEnv_[ch] * peakReleaseCoeff);

        const float tau = (blockMeanSq > outputRmsSq_[ch]) ? 0.30f : 0.40f;
        const float rmsCoeff = std::exp (-dtSec / tau);
        outputRmsSq_[ch] = (rmsCoeff * outputRmsSq_[ch]) + ((1.0f - rmsCoeff) * blockMeanSq);

        const float peakDb = clampStoredDb (linToDb (outputPeakEnv_[ch]));
        float rmsDb = clampStoredDb (linToDb (std::sqrt (outputRmsSq_[ch])));
        rmsDb = juce::jmin (rmsDb, peakDb);

        outputMeters_[ch].peakDb.store (peakDb, std::memory_order_relaxed);
        outputMeters_[ch].rmsDb.store (rmsDb, std::memory_order_relaxed);
        if (clipped)
            outputMeters_[ch].clipLatched.store (true, std::memory_order_relaxed);
    }

    // Hardware meter mapping (RT-safe): convert current meter states to LED-friendly levels.
    {
        HardwareMeterLevelsFrame frame;
        frame.input.channelCount = inChCount;
        frame.output.channelCount = outChCount;

        for (int ch = 0; ch < 2; ++ch)
        {
            const float inPeakDb = inputMeters_[ch].peakDb.load (std::memory_order_relaxed);
            const float inRmsDb  = inputMeters_[ch].rmsDb.load (std::memory_order_relaxed);
            const bool  inClip   = inputMeters_[ch].clipLatched.load (std::memory_order_relaxed);
            frame.input.ch[ch] = hardwareMeterMapper_.mapChannel (inRmsDb, inPeakDb, inClip);

            const float outPeakDb = outputMeters_[ch].peakDb.load (std::memory_order_relaxed);
            const float outRmsDb  = outputMeters_[ch].rmsDb.load (std::memory_order_relaxed);
            const bool  outClip   = outputMeters_[ch].clipLatched.load (std::memory_order_relaxed);
            frame.output.ch[ch] = hardwareMeterMapper_.mapChannel (outRmsDb, outPeakDb, outClip);
        }

        softwareMeterSink_.publishMeterLevels (frame);
    }
}

int AnalayzerProAudioProcessor::getMeterInputChannelCount() const noexcept
{
    return juce::jlimit (1, 2, getTotalNumInputChannels());
}

int AnalayzerProAudioProcessor::getMeterOutputChannelCount() const noexcept
{
    return juce::jlimit (1, 2, getTotalNumOutputChannels());
}

void AnalayzerProAudioProcessor::resetMeterClipLatches() noexcept
{
    for (int ch = 0; ch < 2; ++ch)
    {
        inputMeters_[ch].clipLatched.store (false, std::memory_order_relaxed);
        outputMeters_[ch].clipLatched.store (false, std::memory_order_relaxed);
    }
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
    // No-op: analyzer parameters are polled and applied in processBlock.
    // Keeping this override to satisfy the listener interface (if implemented in the header).
    juce::ignoreUnused (parameterID, newValue);
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
    
    // Analyzer Peak Hold (bool, default true - peaks enabled by default)
    params.push_back (std::make_unique<juce::AudioParameterBool> (
        "PeakHold", "Peak Hold",
        true,  // Default: enabled
        "Peak Hold"));
    
    // Analyzer Hold (bool, default false - freezes decay)
    params.push_back (std::make_unique<juce::AudioParameterBool> (
        "Hold", "Hold",
        false,  // Default: off
        "Hold"));
    
    // Analyzer Peak Decay (0..10 dB/sec, default 1.0 dB/sec)
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "PeakDecay", "Peak Decay",
        juce::NormalisableRange<float> (0.0f, 60.0f, 0.1f),
        1.0f,  // Default: 1.0 dB/sec
        "Peak Decay (dB/s)"));
    
    // Analyzer Display Gain (-24..+24 dB, default 0.0 dB, step 0.5 dB)
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "DisplayGain", "Display Gain",
        juce::NormalisableRange<float> (-24.0f, 24.0f, 0.5f),
        0.0f,  // Default: 0.0 dB
        "Display Gain (dB)"));
    
    // Analyzer Tilt (choice: Flat=0, Pink=1, White=2)
    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        "Tilt", "Tilt",
        juce::StringArray { "Flat", "Pink", "White" },
        0,  // Default: Flat (index 0)
        "Tilt"));

    // Analyzer dB Range (choice: -60=0, -90=1, -120=2)
    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        "DbRange", "dB Range",
        juce::StringArray { "-60 dB", "-90 dB", "-120 dB" },
        2,  // Default: -120 dB (index 2)
        "dB Range"));
    
    return { params.begin(), params.end() };
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AnalayzerProAudioProcessor();
}
