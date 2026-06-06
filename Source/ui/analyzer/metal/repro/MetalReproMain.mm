#include "analyzer/AnalyzerEngine.h"
#include "ui/analyzer/metal/MetalHost.h"

#import <AppKit/AppKit.h>

#include <atomic>
#include <chrono>
#include <cmath>
#include <csignal>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <limits>
#include <memory>
#include <random>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

namespace
{
using namespace AnalyzerPro::metal;
using Clock = std::chrono::steady_clock;

constexpr int kDefaultCycles = 50;
constexpr uint64_t kDefaultFramesPerCycle = 200000;
constexpr int kEditorWidth = 1120;
constexpr int kEditorHeight = 680;
constexpr double kSampleRate = 48000.0;
constexpr int kBlockSize = 512;
constexpr double kPublishHz = 15.0;
constexpr uint32_t kDefaultSeed = 0x4d445350u; // "MDSP"

std::atomic<bool> gShouldStop { false };

enum class HarnessMode
{
    Empty,
    Chrome,
    Analyzer,
    AnalyzerMultiTrace
};

struct HarnessConfig
{
    HarnessMode mode = HarnessMode::Empty;
    int cycles = kDefaultCycles;
    uint64_t framesPerCycle = kDefaultFramesPerCycle;
    uint32_t seed = kDefaultSeed;
};

const char* modeName (HarnessMode mode) noexcept
{
    switch (mode)
    {
        case HarnessMode::Empty:              return "empty";
        case HarnessMode::Chrome:             return "chrome";
        case HarnessMode::Analyzer:           return "analyzer";
        case HarnessMode::AnalyzerMultiTrace: return "analyzer+multitrace";
    }

    return "unknown";
}

void handleSignal (int)
{
    gShouldStop.store (true, std::memory_order_release);
}

bool parseUInt64 (const char* text, uint64_t& value)
{
    if (text == nullptr || *text == '\0')
        return false;

    char* end = nullptr;
    const auto parsed = std::strtoull (text, &end, 10);
    if (end == text || *end != '\0')
        return false;

    value = parsed;
    return true;
}

bool parseInt (const char* text, int& value)
{
    uint64_t parsed = 0;
    if (! parseUInt64 (text, parsed) || parsed > static_cast<uint64_t> (std::numeric_limits<int>::max()))
        return false;

    value = static_cast<int> (parsed);
    return true;
}

std::vector<std::string> collectArgTokens (int argc, char* argv[])
{
    std::vector<std::string> tokens;
    tokens.reserve (static_cast<size_t> (juce::jmax (0, argc - 1)));

    for (int i = 1; i < argc; ++i)
    {
        if (argv[i] == nullptr)
            continue;

        std::istringstream stream (argv[i]);
        std::string token;
        while (stream >> token)
            tokens.push_back (token);
    }

    return tokens;
}

bool parseArgs (int argc, char* argv[], HarnessConfig& config)
{
    const auto args = collectArgTokens (argc, argv);

    for (size_t i = 0; i < args.size(); ++i)
    {
        const std::string& arg = args[i];

        if (arg == "--empty")
        {
            config.mode = HarnessMode::Empty;
        }
        else if (arg == "--chrome")
        {
            config.mode = HarnessMode::Chrome;
        }
        else if (arg == "--analyzer")
        {
            if (config.mode != HarnessMode::AnalyzerMultiTrace)
                config.mode = HarnessMode::Analyzer;
        }
        else if (arg == "--multitrace")
        {
            config.mode = HarnessMode::AnalyzerMultiTrace;
        }
        else if (arg.rfind ("--cycles=", 0) == 0)
        {
            if (! parseInt (arg.c_str() + std::strlen ("--cycles="), config.cycles))
                return false;
        }
        else if (arg == "--cycles" && i + 1 < args.size())
        {
            if (! parseInt (args[++i].c_str(), config.cycles))
                return false;
        }
        else if (arg.rfind ("--frames=", 0) == 0)
        {
            if (! parseUInt64 (arg.c_str() + std::strlen ("--frames="), config.framesPerCycle))
                return false;
        }
        else if (arg == "--frames" && i + 1 < args.size())
        {
            if (! parseUInt64 (args[++i].c_str(), config.framesPerCycle))
                return false;
        }
        else if (arg.rfind ("--seed=", 0) == 0)
        {
            uint64_t seed = 0;
            if (! parseUInt64 (arg.c_str() + std::strlen ("--seed="), seed))
                return false;
            config.seed = static_cast<uint32_t> (seed);
        }
        else if (arg == "--seed" && i + 1 < args.size())
        {
            uint64_t seed = 0;
            if (! parseUInt64 (args[++i].c_str(), seed))
                return false;
            config.seed = static_cast<uint32_t> (seed);
        }
        else if (arg == "--help" || arg == "-h")
        {
            std::cout << "Usage: MetalReproHarness [--empty|--chrome|--analyzer [--multitrace]] "
                         "[--cycles N] [--frames N] [--seed N]\n";
            return false;
        }
        else
        {
            std::cerr << "Unknown argument: " << arg << "\n";
            return false;
        }
    }

    return config.cycles > 0 && config.framesPerCycle > 0;
}

class HarnessComponent final : public juce::Component
{
public:
    HarnessComponent()
    {
        setName ("MetalReproHarnessView");
        setOpaque (true);
        setSize (kEditorWidth, kEditorHeight);
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colours::black);
    }
};

void pumpMainThreadFor (double seconds)
{
    NSDate* stopDate = [NSDate dateWithTimeIntervalSinceNow: seconds];

    while (! gShouldStop.load (std::memory_order_acquire)
           && [stopDate timeIntervalSinceNow] > 0.0)
    {
        @autoreleasepool
        {
            NSDate* eventLimit = [NSDate dateWithTimeIntervalSinceNow: 0.002];
            NSEvent* event = [NSApp nextEventMatchingMask: NSEventMaskAny
                                                untilDate: eventLimit
                                                   inMode: NSDefaultRunLoopMode
                                                  dequeue: YES];
            if (event != nil)
                [NSApp sendEvent: event];

            [NSApp updateWindows];
            CFRunLoopRunInMode (kCFRunLoopDefaultMode, 0.001, true);
        }
    }
}

bool waitForPeer (juce::Component& component)
{
    const auto deadline = Clock::now() + std::chrono::seconds (2);
    while (component.getPeer() == nullptr && Clock::now() < deadline)
        pumpMainThreadFor (0.01);

    return component.getPeer() != nullptr;
}

bool waitForLiveRenderThreads (int expected)
{
    const auto deadline = Clock::now() + std::chrono::seconds (3);
    while (gMetalHostLiveRenderThreads.load (std::memory_order_acquire) != expected
           && Clock::now() < deadline)
    {
        pumpMainThreadFor (0.01);
    }

    return gMetalHostLiveRenderThreads.load (std::memory_order_acquire) == expected;
}

std::shared_ptr<FrameTexturePayload> makeSyntheticChromePayload (int widthPx,
                                                                 int heightPx,
                                                                 float scale,
                                                                 uint64_t sequence)
{
    auto payload = std::make_shared<FrameTexturePayload>();
    payload->widthPx = juce::jmax (1, widthPx);
    payload->heightPx = juce::jmax (1, heightPx);
    payload->bytesPerRow = payload->widthPx * 4;
    payload->scale = scale;
    payload->sequence = sequence;
    payload->bgraPixels.resize (static_cast<size_t> (payload->heightPx)
                                * static_cast<size_t> (payload->bytesPerRow));

    const int boxSize = juce::jmax (24, payload->widthPx / 12);
    const int boxX = static_cast<int> (sequence % static_cast<uint64_t> (payload->widthPx));
    const int boxY = payload->heightPx / 3;

    for (int y = 0; y < payload->heightPx; ++y)
    {
        auto* row = payload->bgraPixels.data() + static_cast<size_t> (y) * static_cast<size_t> (payload->bytesPerRow);
        const uint8_t gy = static_cast<uint8_t> ((y * 255) / payload->heightPx);

        for (int x = 0; x < payload->widthPx; ++x)
        {
            const bool inBox = x >= boxX
                && x < juce::jmin (payload->widthPx, boxX + boxSize)
                && y >= boxY
                && y < juce::jmin (payload->heightPx, boxY + boxSize);

            const uint8_t bx = static_cast<uint8_t> ((x * 255) / payload->widthPx);
            const size_t offset = static_cast<size_t> (x) * 4u;
            row[offset + 0u] = inBox ? 255u : bx;
            row[offset + 1u] = inBox ? 210u : gy;
            row[offset + 2u] = inBox ? 40u : static_cast<uint8_t> ((sequence + static_cast<uint64_t> (x + y)) & 0x7fu);
            row[offset + 3u] = 255u;
        }
    }

    return payload;
}

MetalRectPx insetPlotRect (int widthPx, int heightPx)
{
    const float insetX = static_cast<float> (widthPx) * 0.08f;
    const float insetY = static_cast<float> (heightPx) * 0.08f;
    return { insetX, insetY, static_cast<float> (widthPx) - (2.0f * insetX), static_cast<float> (heightPx) - (2.0f * insetY) };
}

void fillSyntheticTrace (MetalTracePayload& trace,
                         int bins,
                         uint64_t sequence,
                         MetalColour colour,
                         float phaseOffset)
{
    bins = juce::jmax (2, bins);
    trace.visible = true;
    trace.strokeVisible = true;
    trace.fillToBottom = false;
    trace.colour = colour;
    trace.db.resize (static_cast<size_t> (bins));

    for (int i = 0; i < bins; ++i)
    {
        const float t = static_cast<float> (i) / static_cast<float> (bins - 1);
        const float pinkSlope = -18.0f * std::log10 (1.0f + 24.0f * t);
        const float motion = 4.5f * std::sin ((t * 22.0f) + (static_cast<float> (sequence) * 0.055f) + phaseOffset);
        const float ripple = 1.75f * std::sin ((t * 79.0f) + phaseOffset);
        trace.db[static_cast<size_t> (i)] = juce::jlimit (-90.0f, 6.0f, -20.0f + pinkSlope + motion + ripple);
    }
}

std::shared_ptr<MetalAnalyzerFrame> makeSyntheticAnalyzerFrame (MetalRectPx plotRectPx,
                                                               int fftSize,
                                                               double sampleRate,
                                                               uint64_t sequence,
                                                               bool multiTrace)
{
    auto frame = std::make_shared<MetalAnalyzerFrame>();
    frame->valid = true;
    frame->sequence = sequence;
    frame->plotRectPx = plotRectPx;
    frame->minHz = 20.0f;
    frame->maxHz = 20000.0f;
    frame->topDb = 6.0f;
    frame->bottomDb = -90.0f;
    frame->displayGainDb = 0.0f;
    frame->rmsAttackMs = 60.0f;
    frame->rmsReleaseMs = 300.0f;
    frame->sampleRate = sampleRate;
    frame->fftSize = fftSize;
    frame->rmsColour = { 0.18f, 0.74f, 1.0f, 1.0f };
    frame->rmsTrace.visible = true;
    frame->rmsTrace.strokeVisible = true;
    frame->rmsTrace.fillToBottom = true;
    frame->rmsTrace.fillTopAlpha = 0.25f;
    frame->rmsTrace.fillBottomAlpha = 0.0f;
    frame->rmsTrace.colour = frame->rmsColour;

    if (multiTrace)
    {
        const int bins = (fftSize / 2) + 1;
        fillSyntheticTrace (frame->peakTrace, bins, sequence, { 1.0f, 0.80f, 0.18f, 0.85f }, 0.0f);
        fillSyntheticTrace (frame->stereoTrace, bins, sequence, { 0.70f, 0.55f, 1.0f, 0.72f }, 1.3f);
        fillSyntheticTrace (frame->midTrace, bins, sequence, { 0.24f, 1.0f, 0.54f, 0.72f }, 2.6f);
        fillSyntheticTrace (frame->sideTrace, bins, sequence, { 1.0f, 0.35f, 0.44f, 0.72f }, 3.9f);
    }

    return frame;
}

class AudioFeeder final
{
public:
    AudioFeeder (AnalyzerEngine& engineIn, uint32_t seedIn)
        : engine (engineIn), seed (seedIn)
    {
    }

    ~AudioFeeder()
    {
        stop();
    }

    void start()
    {
        shouldExit.store (false, std::memory_order_release);
        thread = std::thread ([this] { run(); });
    }

    void stop()
    {
        shouldExit.store (true, std::memory_order_release);
        if (thread.joinable())
            thread.join();
    }

    int getCurrentFftSize() const noexcept
    {
        return currentFftSize.load (std::memory_order_acquire);
    }

private:
    void run()
    {
        juce::AudioBuffer<float> buffer (2, kBlockSize);
        std::seed_seq seedSequence { seed };
        std::mt19937 rng (seedSequence);
        std::uniform_real_distribution<float> noise (-1.0f, 1.0f);

        const auto blockDuration = std::chrono::duration<double> (static_cast<double> (kBlockSize) / kSampleRate);
        auto nextBlockTime = Clock::now();
        auto nextFftToggle = Clock::now() + std::chrono::seconds (3);

        double sweepPhase = 0.0;
        double sweepPosition = 0.0;
        float pinkL = 0.0f;
        float pinkR = 0.0f;

        while (! shouldExit.load (std::memory_order_acquire)
               && ! gShouldStop.load (std::memory_order_acquire))
        {
            if (Clock::now() >= nextFftToggle)
            {
                const int nextFft = currentFftSize.load (std::memory_order_relaxed) == 2048 ? 4096 : 2048;
                engine.setFftSize (nextFft);
                currentFftSize.store (nextFft, std::memory_order_release);
                nextFftToggle += std::chrono::seconds (3);
            }

            auto* left = buffer.getWritePointer (0);
            auto* right = buffer.getWritePointer (1);

            for (int i = 0; i < kBlockSize; ++i)
            {
                const double sweepT = std::fmod (sweepPosition, kSampleRate * 4.0) / (kSampleRate * 4.0);
                const double freq = 20.0 * std::pow (1000.0, sweepT);
                sweepPhase += (juce::MathConstants<double>::twoPi * freq) / kSampleRate;
                if (sweepPhase > juce::MathConstants<double>::twoPi)
                    sweepPhase -= juce::MathConstants<double>::twoPi;

                pinkL = (0.985f * pinkL) + (0.015f * noise (rng));
                pinkR = (0.982f * pinkR) + (0.018f * noise (rng));
                const float sweep = static_cast<float> (std::sin (sweepPhase));
                const float shiftedSweep = static_cast<float> (std::sin (sweepPhase + 0.37));
                left[i] = (0.10f * pinkL) + (0.14f * sweep);
                right[i] = (0.10f * pinkR) + (0.14f * shiftedSweep);
                sweepPosition += 1.0;
            }

            engine.processBlock (buffer);
            nextBlockTime += std::chrono::duration_cast<Clock::duration> (blockDuration);
            std::this_thread::sleep_until (nextBlockTime);
        }
    }

    AnalyzerEngine& engine;
    uint32_t seed = kDefaultSeed;
    std::atomic<bool> shouldExit { false };
    std::atomic<int> currentFftSize { 2048 };
    std::thread thread;
};

bool runCycle (const HarnessConfig& config, int cycleIndex)
{
    AnalyzerEngine engine;
    engine.prepare (kSampleRate, kBlockSize);
    engine.setFftSize (2048);

    std::unique_ptr<AudioFeeder> feeder;
    if (config.mode == HarnessMode::Analyzer || config.mode == HarnessMode::AnalyzerMultiTrace)
    {
        feeder = std::make_unique<AudioFeeder> (engine, config.seed + static_cast<uint32_t> (cycleIndex));
        feeder->start();
        pumpMainThreadFor (0.10);
    }

    HarnessComponent component;
    const auto* display = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay();
    if (display == nullptr)
    {
        std::cerr << "No display found; CAMetalLayer nextDrawable would hide the repro.\n";
        return false;
    }

    const auto area = display->userArea;
    component.setTopLeftPosition (area.getCentreX() - (kEditorWidth / 2),
                                  area.getCentreY() - (kEditorHeight / 2));
    component.addToDesktop (juce::ComponentPeer::windowHasTitleBar
                            | juce::ComponentPeer::windowIsResizable
                            | juce::ComponentPeer::windowAppearsOnTaskbar);
    component.setVisible (true);
    component.toFront (true);

    if (! waitForPeer (component))
    {
        std::cerr << "Timed out waiting for JUCE peer/native NSView.\n";
        return false;
    }

    if (auto* peer = component.getPeer())
        if (auto* nativeView = static_cast<NSView*> (peer->getNativeHandle()))
            [[nativeView window] makeKeyAndOrderFront: nil];

    MetalHost host;
    if (! host.start (component, MetalHostMechanism::BackingLayer, &engine))
    {
        std::cerr << "MetalHost failed to start.\n";
        component.removeFromDesktop();
        return false;
    }

    host.resized();

    __block uint64_t publishSequence = 1;
    __block MetalHost* timerHost = &host;
    __block HarnessComponent* timerComponent = &component;
    __block AudioFeeder* timerFeeder = feeder.get();
    const bool publishChrome = config.mode == HarnessMode::Chrome
        || config.mode == HarnessMode::Analyzer
        || config.mode == HarnessMode::AnalyzerMultiTrace;
    const bool publishAnalyzer = config.mode == HarnessMode::Analyzer
        || config.mode == HarnessMode::AnalyzerMultiTrace;
    const bool publishMultiTrace = config.mode == HarnessMode::AnalyzerMultiTrace;

    NSTimer* publishTimer = nil;
    if (publishChrome || publishAnalyzer)
    {
        publishTimer = [NSTimer scheduledTimerWithTimeInterval: 1.0 / kPublishHz
                                                       repeats: YES
                                                         block: ^(NSTimer*)
        {
            @autoreleasepool
            {
                const float scale = juce::jmax (1.0f, timerHost->getBackingScaleFactor());
                const int widthPx = juce::jmax (1, juce::roundToInt (static_cast<float> (timerComponent->getWidth()) * scale));
                const int heightPx = juce::jmax (1, juce::roundToInt (static_cast<float> (timerComponent->getHeight()) * scale));

                if (publishChrome)
                    timerHost->setChromeFrame (makeSyntheticChromePayload (widthPx, heightPx, scale, publishSequence));

                if (publishAnalyzer)
                {
                    const int fftSize = timerFeeder != nullptr ? timerFeeder->getCurrentFftSize() : 2048;
                    timerHost->setAnalyzerFrame (makeSyntheticAnalyzerFrame (insetPlotRect (widthPx, heightPx),
                                                                             fftSize,
                                                                             kSampleRate,
                                                                             publishSequence,
                                                                             publishMultiTrace));
                }

                ++publishSequence;
            }
        }];

        [publishTimer fire];
    }

    const uint64_t startFrame = gMetalHostRenderedFrames.load (std::memory_order_acquire);
    const uint64_t targetFrame = startFrame + config.framesPerCycle;
    std::cout << "cycle " << (cycleIndex + 1) << "/" << config.cycles
              << " mode=" << modeName (config.mode)
              << " target_frames=" << config.framesPerCycle << "\n";

    while (! gShouldStop.load (std::memory_order_acquire)
           && gMetalHostRenderedFrames.load (std::memory_order_acquire) < targetFrame)
    {
        pumpMainThreadFor (1.0 / 120.0);
    }

    if (publishTimer != nil)
    {
        [publishTimer invalidate];
        publishTimer = nil;
    }

    if (feeder != nullptr)
        feeder->stop();

    host.stop();
    component.removeFromDesktop();
    pumpMainThreadFor (0.05);

    if (! waitForLiveRenderThreads (0))
    {
        std::cerr << "live_render_threads="
                  << gMetalHostLiveRenderThreads.load (std::memory_order_acquire)
                  << " after teardown\n";
        return false;
    }

    return ! gShouldStop.load (std::memory_order_acquire);
}

} // namespace

int main (int argc, char* argv[])
{
    @autoreleasepool
    {
        std::signal (SIGINT, handleSignal);
        std::signal (SIGTERM, handleSignal);

        HarnessConfig config;
        if (! parseArgs (argc, argv, config))
            return 2;

        juce::ScopedJuceInitialiser_GUI juceInitialiser;

        NSApplication* app = [NSApplication sharedApplication];
        [app setActivationPolicy: NSApplicationActivationPolicyRegular];
        [app finishLaunching];
        [app activateIgnoringOtherApps: YES];

        std::cout << "MetalReproHarness starting: mode=" << modeName (config.mode)
                  << " cycles=" << config.cycles
                  << " frames_per_cycle=" << config.framesPerCycle
                  << " seed=" << config.seed << "\n";

        for (int cycle = 0; cycle < config.cycles; ++cycle)
        {
            if (! runCycle (config, cycle))
                return gShouldStop.load (std::memory_order_acquire) ? 130 : 1;
        }

        std::cout << "MetalReproHarness completed; live_render_threads="
                  << gMetalHostLiveRenderThreads.load (std::memory_order_acquire)
                  << " rendered_frames="
                  << gMetalHostRenderedFrames.load (std::memory_order_acquire)
                  << "\n";
    }

    return 0;
}
