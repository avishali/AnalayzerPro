#include "DeviceRoutingHelper.h"

namespace analyzerpro
{
static bool isNotEmpty(const juce::String& s) { return s.trim().isNotEmpty(); }

juce::String DeviceRoutingHelper::findDeviceNameBySubstring(juce::AudioIODeviceType& type,
                                                            bool isInput,
                                                            const juce::String& substring)
{
    if (! isNotEmpty(substring))
        return {};

    type.scanForDevices();
    const auto names = type.getDeviceNames(isInput);
    const auto needle = substring.toLowerCase();

    for (const auto& n : names)
        if (n.toLowerCase().contains(needle))
            return n;

    return {};
}

void DeviceRoutingHelper::logAvailableDevices(juce::AudioDeviceManager& dm)
{
    auto& types = dm.getAvailableDeviceTypes();

    DBG("---- Audio Device Types ----");
    for (auto* t : types)
    {
        if (t == nullptr) continue;
        t->scanForDevices();
        DBG("Type: " + t->getTypeName());

        const auto ins  = t->getDeviceNames(true);
        const auto outs = t->getDeviceNames(false);

        DBG("  Inputs:");
        for (auto& n : ins)  { juce::ignoreUnused (n); DBG("    - " + n); }

        DBG("  Outputs:");
        for (auto& n : outs) { juce::ignoreUnused (n); DBG("    - " + n); }
    }
    DBG("----------------------------");
}

void DeviceRoutingHelper::logSetup(const juce::AudioDeviceManager::AudioDeviceSetup& s)
{
    juce::ignoreUnused (s);
    DBG("---- AudioDeviceSetup ----");
    DBG(" inputDeviceName  : " + s.inputDeviceName);
    DBG(" outputDeviceName : " + s.outputDeviceName);
    DBG(" sampleRate       : " + juce::String(s.sampleRate));
    DBG(" bufferSize       : " + juce::String(s.bufferSize));
    DBG(" inputChannels    : " + s.inputChannels.toString(2));
    DBG(" outputChannels   : " + s.outputChannels.toString(2));
    DBG("--------------------------");
}

DeviceRoutingHelper::Result DeviceRoutingHelper::applyPreset(juce::AudioDeviceManager& dm,
                                                             const DeviceRoutingPreset& preset,
                                                             bool logDevices)
{
    if (logDevices)
        logAvailableDevices(dm);

    juce::AudioDeviceManager::AudioDeviceSetup setup;
    dm.getAudioDeviceSetup(setup);

    juce::String inputMatch  = preset.inputMatch;
    juce::String outputMatch = preset.outputMatch;

    const auto currentInLower  = setup.inputDeviceName.toLowerCase();
    const auto currentOutLower = setup.outputDeviceName.toLowerCase();

    // Guard: if we're currently on "Multi-Output Device", force BOTH input and output to an
    // Aggregate Device match (Aggregate must be opened as the same CoreAudio device for I/O).
    if (currentInLower.contains("multi-output device") || currentOutLower.contains("multi-output device"))
    {
        juce::String aggregateMatch = outputMatch;
        if (! isNotEmpty(aggregateMatch))
            aggregateMatch = inputMatch;
        if (! isNotEmpty(aggregateMatch))
            aggregateMatch = "BlsckHole 2ch";

        inputMatch  = aggregateMatch;
        outputMatch = aggregateMatch;
    }

    // Choose output device:
    // - if preset.outputMatch provided: select by substring
    // - else: keep existing output name (or let JUCE decide if empty)
    if (isNotEmpty(outputMatch))
    {
        bool found = false;
        auto& availableTypes = dm.getAvailableDeviceTypes();
        for (auto* t : availableTypes)
        {
            if (t == nullptr) continue;
            auto outName = findDeviceNameBySubstring(*t, /*isInput*/ false, outputMatch);
            if (outName.isNotEmpty())
            {
                setup.outputDeviceName = outName;
                found = true;
                break;
            }
        }

        if (! found)
        {
            const auto msg = "Output device not found matching: " + outputMatch;
            DBG(msg);
            return { false, msg };
        }
    }

    // Choose input device (required for capture):
    if (isNotEmpty(inputMatch))
    {
        bool found = false;
        auto& availableTypes = dm.getAvailableDeviceTypes();
        for (auto* t : availableTypes)
        {
            if (t == nullptr) continue;
            auto inName = findDeviceNameBySubstring(*t, /*isInput*/ true, inputMatch);
            if (inName.isNotEmpty())
            {
                setup.inputDeviceName = inName;
                found = true;
                break;
            }
        }

        if (! found)
        {
            const auto msg = "Input device not found matching: " + inputMatch;
            DBG(msg);
            return { false, msg };
        }
    }
    else
    {
        const auto msg = "Preset inputMatch is empty (capture requires an input device).";
        DBG(msg);
        return { false, msg };
    }

    // Channel masks (do not use "default channels" in loopback workflows)
    setup.useDefaultInputChannels = false;
    setup.useDefaultOutputChannels = false;

    setup.inputChannels.clear();
    setup.outputChannels.clear();

    if (preset.inputChannels > 0)
        setup.inputChannels.setRange(0, preset.inputChannels, true);
    if (preset.outputChannels > 0)
        setup.outputChannels.setRange(0, preset.outputChannels, true);

    // Rate / buffer
    if (preset.sampleRate > 0.0)
        setup.sampleRate = preset.sampleRate;
    if (preset.bufferSize > 0)
        setup.bufferSize = preset.bufferSize;

    DBG("Applying routing preset...");
    logSetup(setup);

    const auto err = dm.setAudioDeviceSetup(setup, /*treatAsChosenDevice*/ true);
    if (err.isNotEmpty())
    {
        const auto msg = "setAudioDeviceSetup failed: " + err;
        DBG(msg);
        return { false, msg };
    }

    // Post-check: confirm the current device exists and has active input channels
    if (auto* dev = dm.getCurrentAudioDevice())
    {
        const auto activeIn  = dev->getActiveInputChannels();
        const auto activeOut = dev->getActiveOutputChannels();

        DBG("Current device: " + dev->getName());
        DBG("Active IN : " + activeIn.toString(2));
        DBG("Active OUT: " + activeOut.toString(2));

        if (activeIn.countNumberOfSetBits() == 0)
            return { false, "Device opened but no active input channels (input mask is zero)." };

        // One-time debug: channel names (to confirm channel indices in the aggregate device)
        static bool loggedChannelNamesOnce = false;
        if (! loggedChannelNamesOnce)
        {
            loggedChannelNamesOnce = true;

            DBG("---- Device Channel Names (one-time) ----");
            {
                const auto inNames = dev->getInputChannelNames();
                DBG("Input channel names:");
                for (int i = 0; i < inNames.size(); ++i)
                    DBG("  [" + juce::String(i) + "] " + inNames[i]);
            }
            {
                const auto outNames = dev->getOutputChannelNames();
                DBG("Output channel names:");
                for (int i = 0; i < outNames.size(); ++i)
                    DBG("  [" + juce::String(i) + "] " + outNames[i]);
            }
            DBG("----------------------------------------");
        }
    }
    else
    {
        return { false, "No current audio device after applying setup." };
    }

    return { true, "Routing preset applied successfully." };
}

DeviceRoutingHelper::Result DeviceRoutingHelper::applyMacSystemCapture(juce::AudioDeviceManager& dm,
                                                                       juce::String aggregateMatch,
                                                                       double sampleRate,
                                                                       int bufferSize)
{
    if (! isNotEmpty(aggregateMatch))
        aggregateMatch = "BlackHole 2ch";

    DeviceRoutingPreset p;
    p.inputMatch = aggregateMatch;
    p.outputMatch = aggregateMatch;
    p.sampleRate = sampleRate;
    p.bufferSize = bufferSize;
    p.inputChannels = 2;
    p.outputChannels = 2;

    return applyPreset(dm, p, /*logDevices*/ true);
}

void DeviceRoutingHelper::saveCurrentSetup(juce::AudioDeviceManager& dm, juce::PropertiesFile& props)
{
    juce::AudioDeviceManager::AudioDeviceSetup s;
    dm.getAudioDeviceSetup(s);

    props.setValue("audio.inputDeviceName",  s.inputDeviceName);
    props.setValue("audio.outputDeviceName", s.outputDeviceName);
    props.setValue("audio.sampleRate",       s.sampleRate);
    props.setValue("audio.bufferSize",       s.bufferSize);
    props.setValue("audio.inputChannels",    s.inputChannels.toString(16));
    props.setValue("audio.outputChannels",   s.outputChannels.toString(16));
    props.saveIfNeeded();

    DBG("Saved audio setup to properties.");
}

bool DeviceRoutingHelper::restoreSavedSetup(juce::AudioDeviceManager& dm, juce::PropertiesFile& props)
{
    juce::AudioDeviceManager::AudioDeviceSetup s;
    dm.getAudioDeviceSetup(s);

    const auto inName  = props.getValue("audio.inputDeviceName");
    const auto outName = props.getValue("audio.outputDeviceName");

    if (inName.isEmpty() && outName.isEmpty())
        return false;

    s.inputDeviceName  = inName;
    s.outputDeviceName = outName;
    s.sampleRate = props.getDoubleValue("audio.sampleRate", s.sampleRate);
    s.bufferSize = props.getIntValue("audio.bufferSize", s.bufferSize);

    // Bitset parsing: store as hex string (base16)
    const auto inBitsHex  = props.getValue("audio.inputChannels");
    const auto outBitsHex = props.getValue("audio.outputChannels");
    if (inBitsHex.isNotEmpty())  s.inputChannels.parseString(inBitsHex, 16);
    if (outBitsHex.isNotEmpty()) s.outputChannels.parseString(outBitsHex, 16);

    s.useDefaultInputChannels = false;
    s.useDefaultOutputChannels = false;

    DBG("Restoring audio setup from properties...");
    logSetup(s);

    const auto err = dm.setAudioDeviceSetup(s, true);
    if (err.isNotEmpty())
    {
        DBG("Restore failed: " + err);
        return false;
    }

    return true;
}

//==============================================================================
DeviceRoutingHelper::DeviceRoutingHelper (juce::AudioDeviceManager& dm, juce::ApplicationProperties& props)
    : deviceManager (dm), appProps_ (props)
{
    // Step 2: Restore device state on init
    if (auto* settings = appProps_.getUserSettings())
    {
        const auto xmlString = settings->getValue (deviceStateKey_);
        if (xmlString.isNotEmpty())
        {
            auto xml = juce::XmlDocument::parse (xmlString);
            if (xml != nullptr)
            {
                dm.initialise (256, 256, xml.get(), true);
                DBG ("[DeviceRoutingHelper] Restored state from XML.");
            }
        }
    }

    // Register listener
    deviceManager.addChangeListener (this);
}

DeviceRoutingHelper::~DeviceRoutingHelper()
{
    deviceManager.removeChangeListener (this);
}

void DeviceRoutingHelper::changeListenerCallback (juce::ChangeBroadcaster*)
{
    // Step 3: Save state on device change
    if (auto* settings = appProps_.getUserSettings())
    {
        if (auto xml = deviceManager.createStateXml())
        {
            settings->setValue (deviceStateKey_, xml->toString());
            appProps_.saveIfNeeded();
            DBG ("[DeviceRoutingHelper] Saved state on change.");
        }
    }
}

} // namespace analyzerpro
