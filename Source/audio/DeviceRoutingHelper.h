#pragma once
#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_data_structures/juce_data_structures.h>

namespace analyzerpro
{
struct DeviceRoutingPreset
{
    // Substring matches (case-insensitive). Examples:
    // inputMatch  = "BlackHole"
    // outputMatch = "MacBook Pro Speakers"  (or your interface name)
    juce::String inputMatch;
    juce::String outputMatch;

    double sampleRate = 48000.0;
    int bufferSize = 256;

    int inputChannels = 2;   // how many channels to enable starting from ch0
    int outputChannels = 4;
};

class DeviceRoutingHelper
{
public:
    struct Result
    {
        bool ok = false;
        juce::String message;
    };

    // Configure the device manager for an asymmetric I/O routing preset.
    // If outputMatch is empty, it will keep the current output device (or fall back to default).
    static Result applyPreset(juce::AudioDeviceManager& dm,
                              const DeviceRoutingPreset& preset,
                              bool logDevices = true);

    // Convenience: macOS loopback for Aggregate Devices.
    // IMPORTANT: When using an Aggregate Device, CoreAudio/JUCE must open the SAME device for
    // input and output, otherwise input buffers can be zeroed.
    static Result applyMacSystemCapture(juce::AudioDeviceManager& dm,
                                        juce::String aggregateMatch = "AnalyzerPro Aggregate",
                                        double sampleRate = 48000.0,
                                        int bufferSize = 256);

    // Persistence helpers (optional)
    static void saveCurrentSetup(juce::AudioDeviceManager& dm, juce::PropertiesFile& props);
    static bool restoreSavedSetup(juce::AudioDeviceManager& dm, juce::PropertiesFile& props);

private:
    static juce::String findDeviceNameBySubstring(juce::AudioIODeviceType& type,
                                                 bool isInput,
                                                 const juce::String& substring);

    static void logAvailableDevices(juce::AudioDeviceManager& dm);
    static void logSetup(const juce::AudioDeviceManager::AudioDeviceSetup& s);
};
} // namespace analyzerpro
