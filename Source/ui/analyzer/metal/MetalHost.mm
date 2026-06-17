#include "MetalHost.h"
#include "../../../analyzer/AnalyzerEngine.h"

#if JUCE_MAC

#import <AppKit/AppKit.h>
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>
#import <QuartzCore/QuartzCore.h>
#include <simd/simd.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstddef>
#include <limits>
#include <memory>
#include <thread>
#include <vector>

#include <mdsp_ui/rta/MultiTraceState.h>

#if !defined(ANALYZERPRO_METAL_DIAGNOSTICS)
#define ANALYZERPRO_METAL_DIAGNOSTICS 0
#endif

namespace AnalyzerPro::metal
{
struct MetalHostImpl;
}

@interface AnalyzerProMetalCoverView : NSView
{
@public
    AnalyzerPro::metal::MetalHostImpl* owner;
}
@end

namespace AnalyzerPro::metal
{
namespace
{
id<MTLDevice> getSharedDevice()
{
    static id<MTLDevice> device = MTLCreateSystemDefaultDevice();
    return device;
}

int nextMetalHostInstanceId() noexcept
{
    static std::atomic<int> nextId { 1 };
    return nextId.fetch_add (1, std::memory_order_relaxed);
}

// Process-wide count of live render threads. Incremented when a render thread starts and
// decremented when it exits, so the close/reopen zombie-accumulation gate can confirm the
// count returns to 0 after each editor close (no abandoned render threads).
std::atomic<int>& liveRenderThreadCount() noexcept
{
    return gMetalHostLiveRenderThreads;
}

struct ChromeVertex
{
    float position[2];
    float texCoord[2];
};

struct ColourVertex
{
    float position[2];
    float colour[4];
};

constexpr size_t kMaxAnalyzerBins = static_cast<size_t> (AnalyzerSnapshot::kMaxFFTBins);
constexpr size_t kMaxAnalyzerFillVertices = kMaxAnalyzerBins * 2;
constexpr size_t kMaxAnalyzerTraceSlots = 40; // max line/fill draws per frame (bottom-fill + glow/core ribbons)
constexpr size_t kPhaseFanAngleBins = MetalAnalyzerFrame::kPhaseFanAngleBins;
constexpr size_t kMaxPhaseFanFillVertices = (kPhaseFanAngleBins - 1) * 3;
constexpr size_t kMaxPhaseFanRibbonVertices = kPhaseFanAngleBins * 4;
constexpr size_t kMaxGonioPoints = MetalAnalyzerFrame::kGonioMaxPoints;
constexpr size_t kMaxGonioHistoryPoints = MetalAnalyzerFrame::kGonioHistoryPointCapacity;
constexpr size_t kMaxGonioHoldPoints = MetalAnalyzerFrame::kGonioHoldBins;
constexpr size_t kMaxGonioPointQuads = kMaxGonioPoints * 2 + kMaxGonioHistoryPoints + kMaxGonioHoldPoints;
constexpr size_t kMaxGonioPointVertices = kMaxGonioPointQuads * 6;
constexpr size_t kMaxMeterBarVertices = MetalAnalyzerFrame::kMaxMeters * 7 * 6;
constexpr size_t kMaxCrosshairVertices = 18;
constexpr size_t kChromeTextureRingSize = 3;
constexpr int kNoChromeTextureIndex = -1;
constexpr int kMeterDisplayModeRms = 0;
constexpr auto kRenderThreadStopTimeout = std::chrono::milliseconds (2000);
constexpr auto kDrawableUnavailableSleep = std::chrono::milliseconds (2);
#if ANALYZERPRO_METAL_DIAGNOSTICS
constexpr const char* kMetalHostStatsPath = "/tmp/metalhost_stats.txt";
#endif
constexpr size_t kRmsInterpolationIntervalHistorySize = 4;
constexpr double kRmsInterpolationMinIntervalSeconds = 0.001;
constexpr double kRmsInterpolationMaxIntervalSeconds = 0.100;

const char* getRuntimeShaderSource() noexcept
{
    return R"METAL(
#include <metal_stdlib>
using namespace metal;

struct ChromeVertexIn
{
    float2 position [[attribute(0)]];
    float2 texCoord [[attribute(1)]];
};

struct ChromeVertexOut
{
    float4 position [[position]];
    float2 texCoord;
};

vertex ChromeVertexOut analyzerproChromeVertex (ChromeVertexIn in [[stage_in]])
{
    ChromeVertexOut out;
    out.position = float4 (in.position, 0.0, 1.0);
    out.texCoord = in.texCoord;
    return out;
}

fragment float4 analyzerproChromeFragment (ChromeVertexOut in [[stage_in]],
                                           texture2d<float> chrome [[texture(0)]],
                                           sampler chromeSampler [[sampler(0)]])
{
    return chrome.sample (chromeSampler, in.texCoord);
}

struct ColourVertexIn
{
    float2 position [[attribute(0)]];
    float4 colour [[attribute(1)]];
};

struct ColourVertexOut
{
    float4 position [[position]];
    float4 colour;
};

vertex ColourVertexOut analyzerproColourVertex (ColourVertexIn in [[stage_in]])
{
    ColourVertexOut out;
    out.position = float4 (in.position, 0.0, 1.0);
    out.colour = in.colour;
    return out;
}

fragment float4 analyzerproColourFragment (ColourVertexOut in [[stage_in]])
{
    return in.colour;
}
)METAL";
}

juce::ModifierKeys makeModifiers (NSEvent* event)
{
    int flags = juce::ModifierKeys::noModifiers;
    const NSEventModifierFlags eventFlags = [event modifierFlags];

    if ((eventFlags & NSEventModifierFlagShift) != 0)
        flags |= juce::ModifierKeys::shiftModifier;
    if ((eventFlags & NSEventModifierFlagControl) != 0)
        flags |= juce::ModifierKeys::ctrlModifier;
    if ((eventFlags & NSEventModifierFlagOption) != 0)
        flags |= juce::ModifierKeys::altModifier;
    if ((eventFlags & NSEventModifierFlagCommand) != 0)
        flags |= juce::ModifierKeys::commandModifier;

    const NSUInteger buttons = [NSEvent pressedMouseButtons];
    if ((buttons & 1u) != 0)
        flags |= juce::ModifierKeys::leftButtonModifier;
    if ((buttons & 2u) != 0)
        flags |= juce::ModifierKeys::rightButtonModifier;
    if ((buttons & 4u) != 0)
        flags |= juce::ModifierKeys::middleButtonModifier;

    const auto type = [event type];
    if (type == NSEventTypeLeftMouseDown || type == NSEventTypeLeftMouseDragged)
        flags |= juce::ModifierKeys::leftButtonModifier;
    else if (type == NSEventTypeRightMouseDown || type == NSEventTypeRightMouseDragged)
        flags |= juce::ModifierKeys::rightButtonModifier;
    else if (type == NSEventTypeOtherMouseDown || type == NSEventTypeOtherMouseDragged)
        flags |= juce::ModifierKeys::middleButtonModifier;
    else if (type == NSEventTypeLeftMouseUp)
        flags &= ~juce::ModifierKeys::leftButtonModifier;
    else if (type == NSEventTypeRightMouseUp)
        flags &= ~juce::ModifierKeys::rightButtonModifier;
    else if (type == NSEventTypeOtherMouseUp)
        flags &= ~juce::ModifierKeys::middleButtonModifier;

    return juce::ModifierKeys (flags);
}

NSPoint pointInView (NSView* view, NSEvent* event)
{
    return [view convertPoint: [event locationInWindow] fromView: nil];
}

float railClipRightPx (const MetalAnalyzerFrame* frame, float defaultRight) noexcept
{
    if (frame == nullptr || ! frame->railOverlayActive)
        return defaultRight;

    return juce::jmin (defaultRight, frame->railOverlayRectPx.x);
}

} // namespace

struct MetalHostImpl final : private juce::ComponentMovementWatcher
{
    MetalHostImpl (juce::Component& editorIn, MetalHostMechanism mechanismIn, const AnalyzerEngine* analyzerEngineIn)
        : juce::ComponentMovementWatcher (&editorIn),
          editor (editorIn),
          analyzerEngine (analyzerEngineIn),
          mechanism (mechanismIn),
          instanceId (nextMetalHostInstanceId())
    {
        gMetalHostMechanism.store (static_cast<int> (mechanism), std::memory_order_relaxed);
    }

    ~MetalHostImpl() override
    {
        (void) stop();
    }

    bool start()
    {
        if (running)
            return true;

        device = getSharedDevice();
        if (device == nil)
            return false;

        if (commandQueue == nil)
            commandQueue = [device newCommandQueue];
        if (commandQueue == nil)
            return false;

        if (metalLayer == nil)
            metalLayer = [[CAMetalLayer layer] retain];
        if (metalLayer == nil)
            return false;

        metalLayer.device = device;
        metalLayer.pixelFormat = MTLPixelFormatBGRA8Unorm;
        metalLayer.framebufferOnly = YES;
        metalLayer.opaque = YES;
        metalLayer.allowsNextDrawableTimeout = YES;
        metalLayer.presentsWithTransaction = NO;

        if (renderPassDescriptor == nil)
            renderPassDescriptor = [[MTLRenderPassDescriptor renderPassDescriptor] retain];
        if (renderPassDescriptor == nil)
            return false;

        initialiseRenderPipelines();
        initialised = true;

        // Attach + start the render thread now IF the editor already has a native peer. At
        // editor-construction time it usually does NOT — JUCE assigns the peer AFTER the
        // constructor returns — so this attach legitimately defers and completes later in
        // componentPeerChanged(). Metal being *available* (device/queue/layer created) is what
        // counts as success here, NOT being attached yet. We return false only on a genuine
        // Metal-resource failure (preserving the CPU fallback). This restores the Phase-0
        // behaviour that 1A regressed by treating "peer not ready" as fatal and tearing the
        // host down before the peer-attach retry could fire.
        (void) tryAttachAndStartRenderThread();
        return true;
    }

    // Attaches the Metal layer to the editor's native peer view and starts the render thread.
    // Safe to call repeatedly: a no-op once running, and a no-op (returns false) while the peer
    // is not yet available. Called from start() and re-tried from componentPeerChanged().
    bool tryAttachAndStartRenderThread()
    {
        if (running)
            return true;
        if (! initialised)
            return false;

        if (! attachIfPossible())
            return false; // peer not ready yet — componentPeerChanged() will retry

        stopping.store (false, std::memory_order_release);
        if (! startRenderThread())
            return false;

        running = true;
        NSLog (@"[MetalHost #%d] driver=self_paced_nextDrawable render_thread=dedicated", instanceId);
        return true;
    }

    bool stop()
    {
        if (teardownAbandoned)
            return false;

        stopping.store (true, std::memory_order_release);

        // Phase 1 (synchronous, fast, MUST NOT block): restore the peer's original backing layer
        // FIRST. This runs from ~MetalEditorRenderer, before the base Component destructor's
        // removeFromDesktop, so the editor + peer are still alive. Detaching first (a) guarantees a
        // clean peer on reopen even if the drain below times out, and (b) orphans metalLayer so the
        // render thread's in-flight present no longer needs the view's main-thread CoreAnimation
        // transaction -> it completes and the render thread exits promptly (drain won't abandon).
        NSLog (@"[MetalHost #%d] stop(): phase1 detach peer layer", instanceId);
        detachFromPeerView (true);
        clearPublishedFrames();

        if (! stopRenderThreadAndDrain ("stop"))
            return false;

        NSLog (@"[MetalHost #%d] stop(): teardown clean", instanceId);

        if (renderPassDescriptor != nil)
        {
            [renderPassDescriptor release];
            renderPassDescriptor = nil;
        }

        if (chromePipeline != nil)
        {
            [chromePipeline release];
            chromePipeline = nil;
        }

        if (colourPipeline != nil)
        {
            [colourPipeline release];
            colourPipeline = nil;
        }

        if (shaderLibrary != nil)
        {
            [shaderLibrary release];
            shaderLibrary = nil;
        }

        if (chromeSampler != nil)
        {
            [chromeSampler release];
            chromeSampler = nil;
        }

        if (chromeQuadBuffer != nil)
        {
            [chromeQuadBuffer release];
            chromeQuadBuffer = nil;
        }

        if (analyzerFillBuffer != nil)
        {
            [analyzerFillBuffer release];
            analyzerFillBuffer = nil;
        }

        if (analyzerLineBuffer != nil)
        {
            [analyzerLineBuffer release];
            analyzerLineBuffer = nil;
        }

        if (phaseFanFillBuffer != nil)
        {
            [phaseFanFillBuffer release];
            phaseFanFillBuffer = nil;
        }

        if (phaseFanLineBuffer != nil)
        {
            [phaseFanLineBuffer release];
            phaseFanLineBuffer = nil;
        }

        if (gonioPointBuffer != nil)
        {
            [gonioPointBuffer release];
            gonioPointBuffer = nil;
        }

        if (meterBarBuffer != nil)
        {
            [meterBarBuffer release];
            meterBarBuffer = nil;
        }

        if (crosshairBuffer != nil)
        {
            [crosshairBuffer release];
            crosshairBuffer = nil;
        }

        if (metalLayer != nil)
        {
            [metalLayer release];
            metalLayer = nil;
        }

        releaseChromeTextureRing();

        if (commandQueue != nil)
        {
            [commandQueue release];
            commandQueue = nil;
        }

        peerView = nil;
        running = false;
        initialised = false;
        renderPipelinesReady = false;
        analyzerPipelinePrimed = false;
        analyzerPipelineBins = 0;
        analyzerPipelineSampleRate = 0.0;
        analyzerPipelineFftSize = 0;
        analyzerRmsInterpolationStartTime = 0.0;
        analyzerRmsLastSnapshotTime = 0.0;
        analyzerRmsMeasuredIntervalSeconds = 0.0;
        analyzerRmsIntervalHistory.fill (0.0);
        analyzerRmsIntervalHistoryCount = 0;
        analyzerRmsIntervalHistoryIndex = 0;
        lastRenderFrameTime = 0.0;
        gMetalHostFps.store (0.0f, std::memory_order_relaxed);
        gMetalHostEncodeMs.store (0.0f, std::memory_order_relaxed);
        return true;
    }

    bool isRunning() const noexcept
    {
        return running;
    }

    MetalHostMechanism getMechanism() const noexcept
    {
        return mechanism;
    }

    void resized()
    {
        updateLayerGeometry();
    }

    void clearPublishedFrames()
    {
        std::atomic_store_explicit (&latestChromeFrame,
                                    std::shared_ptr<const FrameTexturePayload>(),
                                    std::memory_order_release);
        std::atomic_store_explicit (&latestAnalyzerFrame,
                                    std::shared_ptr<const MetalAnalyzerFrame>(),
                                    std::memory_order_release);
    }

    void setChromeFrame (std::shared_ptr<const FrameTexturePayload> frame)
    {
        std::atomic_store_explicit (&latestChromeFrame, std::move (frame), std::memory_order_release);
    }

    void setAnalyzerFrame (std::shared_ptr<const MetalAnalyzerFrame> frame)
    {
        std::atomic_store_explicit (&latestAnalyzerFrame, std::move (frame), std::memory_order_release);
    }

    float getBackingScaleFactor() const noexcept
    {
        return backingScale.load (std::memory_order_relaxed);
    }

    void componentMovedOrResized (bool, bool) override
    {
        updateLayerGeometry();
    }

    void componentPeerChanged() override
    {
        if (editor.getPeer() == nullptr)
        {
            // Peer gone: peerView may already be freed -> don't touch it (restoreLayer=false),
            // and detach unconditionally so our own references are always cleared.
            (void) stopRenderThreadAndDrain ("peer lost");
            detachFromPeerView (false);
            clearPublishedFrames();
            gMetalHostFps.store (0.0f, std::memory_order_relaxed);
            return;
        }

        if (! running)
        {
            detachFromPeerView();
            if (! teardownAbandoned)
                (void) tryAttachAndStartRenderThread();
            return;
        }

        if (stopRenderThreadAndDrain ("peer changed"))
        {
            detachFromPeerView();
            (void) tryAttachAndStartRenderThread();
        }
    }

    void componentVisibilityChanged() override
    {
        if (editor.getPeer() == nullptr)
        {
            stopRenderThreadAndDrain ("visibility peer lost");
            gMetalHostFps.store (0.0f, std::memory_order_relaxed);
            return;
        }

        if (! editor.isShowing())
        {
            stopRenderThreadAndDrain ("hidden");
            gMetalHostFps.store (0.0f, std::memory_order_relaxed);
            return;
        }

        if (! running && ! teardownAbandoned)
            (void) tryAttachAndStartRenderThread();
    }

    void forwardMouseEvent (AnalyzerProMetalCoverView* view, NSEvent* event)
    {
        if (auto* peer = editor.getPeer())
        {
            const NSPoint local = pointInView (view, event);
            const juce::Point<float> position (static_cast<float> (local.x),
                                               static_cast<float> (local.y));

            peer->handleMouseEvent (juce::MouseInputSource::InputSourceType::mouse,
                                    position,
                                    makeModifiers (event),
                                    0.0f,
                                    0.0f,
                                    juce::Time::currentTimeMillis());
        }
    }

    void forwardWheelEvent (AnalyzerProMetalCoverView* view, NSEvent* event)
    {
        if (auto* peer = editor.getPeer())
        {
            const NSPoint local = pointInView (view, event);
            const juce::Point<float> position (static_cast<float> (local.x),
                                               static_cast<float> (local.y));
            const juce::MouseWheelDetails wheel {
                static_cast<float> ([event scrollingDeltaX] / 120.0),
                static_cast<float> ([event scrollingDeltaY] / 120.0),
                [event isDirectionInvertedFromDevice] != NO,
                [event hasPreciseScrollingDeltas] != NO,
                [event momentumPhase] != NSEventPhaseNone
            };

            peer->handleMouseWheel (juce::MouseInputSource::InputSourceType::mouse,
                                    position,
                                    juce::Time::currentTimeMillis(),
                                    wheel);
        }
    }

    void forwardKeyDown (NSEvent* event)
    {
        if (auto* peer = editor.getPeer())
        {
            NSString* chars = [event charactersIgnoringModifiers];
            const juce::juce_wchar textChar = ([chars length] > 0)
                ? static_cast<juce::juce_wchar> ([chars characterAtIndex: 0])
                : 0;
            peer->handleKeyPress (juce::KeyPress (static_cast<int> (textChar), makeModifiers (event), textChar));
        }
    }

    void forwardKeyUp (NSEvent*)
    {
        if (auto* peer = editor.getPeer())
            peer->handleKeyUpOrDown (false);
    }

private:
    bool startRenderThread()
    {
        if (peerView == nil)
            return false;

        resetStatsCounters();
        renderThreadShouldExit.store (false, std::memory_order_release);
        renderThreadExited.store (false, std::memory_order_release);

        renderThread = std::thread ([this]
        {
            renderThreadMain();
        });

        return true;
    }

    void renderThreadMain()
    {
        const int liveOnEntry = liveRenderThreadCount().fetch_add (1, std::memory_order_acq_rel) + 1;
        NSLog (@"[MetalHost #%d] render thread started (live_render_threads=%d)", instanceId, liveOnEntry);

        while (! renderThreadShouldExit.load (std::memory_order_acquire)
               && ! stopping.load (std::memory_order_acquire))
        {
            if (metalLayer == nil || commandQueue == nil)
            {
                std::this_thread::sleep_for (kDrawableUnavailableSleep);
                continue;
            }

            inFlightFrames.fetch_add (1, std::memory_order_acq_rel);

            id<CAMetalDrawable> drawable = nil;
            const double nextDrawableStart = CACurrentMediaTime();
            @autoreleasepool
            {
                drawable = [[metalLayer nextDrawable] retain];
            }
            nextDrawableBlockedSeconds += CACurrentMediaTime() - nextDrawableStart;

            if (renderThreadShouldExit.load (std::memory_order_acquire)
                || stopping.load (std::memory_order_acquire))
            {
                [drawable release];
                inFlightFrames.fetch_sub (1, std::memory_order_acq_rel);
                break;
            }

            if (drawable == nil)
            {
                inFlightFrames.fetch_sub (1, std::memory_order_acq_rel);
                std::this_thread::sleep_for (kDrawableUnavailableSleep);
                continue;
            }

            @autoreleasepool
            {
                renderFrame (drawable);
            }
            inFlightFrames.fetch_sub (1, std::memory_order_acq_rel);
        }

        renderThreadExited.store (true, std::memory_order_release);
        const int liveOnExit = liveRenderThreadCount().fetch_sub (1, std::memory_order_acq_rel) - 1;
        NSLog (@"[MetalHost #%d] render thread exited (live_render_threads=%d)", instanceId, liveOnExit);
    }

    bool stopRenderThreadAndDrain (const char* reason)
    {
        stopping.store (true, std::memory_order_release);
        renderThreadShouldExit.store (true, std::memory_order_release);
        running = false;

        if (renderThread.joinable())
        {
            if (renderThread.get_id() == std::this_thread::get_id())
            {
                teardownAbandoned = true;
                renderThread.detach();
                NSLog (@"[MetalHost #%d] abandoned teardown from render thread (%s)", instanceId, reason);
                return false;
            }
            else
            {
                const auto deadline = std::chrono::steady_clock::now() + kRenderThreadStopTimeout;
                while (! renderThreadExited.load (std::memory_order_acquire)
                       && std::chrono::steady_clock::now() < deadline)
                {
                    std::this_thread::sleep_for (kDrawableUnavailableSleep);
                }

                if (! renderThreadExited.load (std::memory_order_acquire))
                {
                    teardownAbandoned = true;
                    renderThread.detach();
                    NSLog (@"[MetalHost #%d] abandoned teardown after timeout (%s)", instanceId, reason);
                    return false;
                }

                renderThread.join();
            }
        }

        (void) reason;
        lastRenderFrameTime = 0.0;
        return true;
    }

    bool attachIfPossible()
    {
        NSView* targetPeerView = nil;
        if (auto* peer = editor.getPeer())
            targetPeerView = static_cast<NSView*> (peer->getNativeHandle());

        if (targetPeerView == nil || metalLayer == nil)
            return false;

        if (mechanism == MetalHostMechanism::BackingLayer)
        {
            if (layerAttached && peerView == targetPeerView)
            {
                updateLayerGeometry();
                return true;
            }

            if (layerAttached)
                detachFromPeerView();

            peerView = targetPeerView;
            originalWantsLayer = [peerView wantsLayer];
            originalLayer = [[peerView layer] retain];

            [peerView setLayer: metalLayer];
            [peerView setWantsLayer: YES];
            layerAttached = true;
            updateLayerGeometry();
            return true;
        }

        if (layerAttached)
            detachFromPeerView();

        peerView = targetPeerView;
        coverView = [[AnalyzerProMetalCoverView alloc] initWithFrame: [peerView bounds]];
        if (coverView == nil)
            return false;

        coverView->owner = this;
        [coverView setWantsLayer: YES];
        [coverView setLayer: metalLayer];
        [coverView setAutoresizingMask: NSViewWidthSizable | NSViewHeightSizable];
        [peerView addSubview: coverView positioned: NSWindowAbove relativeTo: nil];
        layerAttached = true;
        updateLayerGeometry();
        return true;
    }

    // restoreLayer=false when the peer / native view is already gone (peerView may be dangling):
    // skip touching the view, just drop our own references.
    void detachFromPeerView (bool restoreLayer = true)
    {
        if (coverView != nil)
        {
            coverView->owner = nullptr;
            if (restoreLayer)
                [coverView removeFromSuperview];
            [coverView release];
            coverView = nil;
        }

        if (restoreLayer && peerView != nil && mechanism == MetalHostMechanism::BackingLayer)
        {
            [peerView setLayer: originalLayer];
            [peerView setWantsLayer: originalWantsLayer];
        }

        if (originalLayer != nil)
        {
            [originalLayer release];
            originalLayer = nil;
        }

        peerView = nil;
        layerAttached = false;
    }

    NSView* getHostView() const noexcept
    {
        return (coverView != nil) ? coverView : peerView;
    }

    void updateLayerGeometry()
    {
        if (metalLayer == nil)
            return;

        NSView* hostView = getHostView();
        if (hostView == nil)
            return;

        const NSRect bounds = [hostView bounds];
        [metalLayer setFrame: bounds];

        const CGFloat scale = ([[hostView window] backingScaleFactor] > 0.0)
            ? [[hostView window] backingScaleFactor]
            : [NSScreen mainScreen].backingScaleFactor;

        metalLayer.contentsScale = scale;
        metalLayer.drawableSize = CGSizeMake (bounds.size.width * scale, bounds.size.height * scale);
        backingScale.store (static_cast<float> (scale), std::memory_order_relaxed);
    }

    void initialiseRenderPipelines()
    {
        if (renderPipelinesReady || device == nil)
            return;

        NSError* error = nil;
        shaderLibrary = [device newDefaultLibrary];
        const bool defaultLibraryWasFound = (shaderLibrary != nil);
        if (shaderLibrary == nil)
        {
            NSString* source = [NSString stringWithUTF8String: getRuntimeShaderSource()];
            shaderLibrary = [device newLibraryWithSource: source options: nil error: &error];
            if (shaderLibrary == nil)
            {
                NSLog (@"[MetalHost #%d] shader library build failed: %@", instanceId, error);
                return;
            }
        }

        id<MTLFunction> chromeVertex = [shaderLibrary newFunctionWithName: @"analyzerproChromeVertex"];
        id<MTLFunction> chromeFragment = [shaderLibrary newFunctionWithName: @"analyzerproChromeFragment"];
        id<MTLFunction> colourVertex = [shaderLibrary newFunctionWithName: @"analyzerproColourVertex"];
        id<MTLFunction> colourFragment = [shaderLibrary newFunctionWithName: @"analyzerproColourFragment"];

        if (chromeVertex == nil || chromeFragment == nil || colourVertex == nil || colourFragment == nil)
        {
            [chromeVertex release];
            [chromeFragment release];
            [colourVertex release];
            [colourFragment release];

            if (! defaultLibraryWasFound)
            {
                NSLog (@"[MetalHost #%d] shader functions missing", instanceId);
                return;
            }

            [shaderLibrary release];
            NSString* source = [NSString stringWithUTF8String: getRuntimeShaderSource()];
            shaderLibrary = [device newLibraryWithSource: source options: nil error: &error];
            if (shaderLibrary == nil)
            {
                NSLog (@"[MetalHost #%d] runtime shader fallback failed: %@", instanceId, error);
                return;
            }

            chromeVertex = [shaderLibrary newFunctionWithName: @"analyzerproChromeVertex"];
            chromeFragment = [shaderLibrary newFunctionWithName: @"analyzerproChromeFragment"];
            colourVertex = [shaderLibrary newFunctionWithName: @"analyzerproColourVertex"];
            colourFragment = [shaderLibrary newFunctionWithName: @"analyzerproColourFragment"];
            if (chromeVertex == nil || chromeFragment == nil || colourVertex == nil || colourFragment == nil)
            {
                NSLog (@"[MetalHost #%d] runtime shader functions missing", instanceId);
                [chromeVertex release];
                [chromeFragment release];
                [colourVertex release];
                [colourFragment release];
                return;
            }
        }

        MTLRenderPipelineDescriptor* chromeDescriptor = [[MTLRenderPipelineDescriptor alloc] init];
        chromeDescriptor.vertexFunction = chromeVertex;
        chromeDescriptor.fragmentFunction = chromeFragment;
        chromeDescriptor.colorAttachments[0].pixelFormat = MTLPixelFormatBGRA8Unorm;
        chromeDescriptor.vertexDescriptor = [MTLVertexDescriptor vertexDescriptor];
        chromeDescriptor.vertexDescriptor.attributes[0].format = MTLVertexFormatFloat2;
        chromeDescriptor.vertexDescriptor.attributes[0].offset = 0;
        chromeDescriptor.vertexDescriptor.attributes[0].bufferIndex = 0;
        chromeDescriptor.vertexDescriptor.attributes[1].format = MTLVertexFormatFloat2;
        chromeDescriptor.vertexDescriptor.attributes[1].offset = static_cast<NSUInteger> (offsetof (ChromeVertex, texCoord));
        chromeDescriptor.vertexDescriptor.attributes[1].bufferIndex = 0;
        chromeDescriptor.vertexDescriptor.layouts[0].stride = sizeof (ChromeVertex);

        chromePipeline = [device newRenderPipelineStateWithDescriptor: chromeDescriptor error: &error];
        [chromeDescriptor release];

        MTLRenderPipelineDescriptor* colourDescriptor = [[MTLRenderPipelineDescriptor alloc] init];
        colourDescriptor.vertexFunction = colourVertex;
        colourDescriptor.fragmentFunction = colourFragment;
        colourDescriptor.colorAttachments[0].pixelFormat = MTLPixelFormatBGRA8Unorm;
        colourDescriptor.colorAttachments[0].blendingEnabled = YES;
        colourDescriptor.colorAttachments[0].rgbBlendOperation = MTLBlendOperationAdd;
        colourDescriptor.colorAttachments[0].alphaBlendOperation = MTLBlendOperationAdd;
        colourDescriptor.colorAttachments[0].sourceRGBBlendFactor = MTLBlendFactorSourceAlpha;
        colourDescriptor.colorAttachments[0].sourceAlphaBlendFactor = MTLBlendFactorSourceAlpha;
        colourDescriptor.colorAttachments[0].destinationRGBBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
        colourDescriptor.colorAttachments[0].destinationAlphaBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
        colourDescriptor.vertexDescriptor = [MTLVertexDescriptor vertexDescriptor];
        colourDescriptor.vertexDescriptor.attributes[0].format = MTLVertexFormatFloat2;
        colourDescriptor.vertexDescriptor.attributes[0].offset = 0;
        colourDescriptor.vertexDescriptor.attributes[0].bufferIndex = 0;
        colourDescriptor.vertexDescriptor.attributes[1].format = MTLVertexFormatFloat4;
        colourDescriptor.vertexDescriptor.attributes[1].offset = static_cast<NSUInteger> (offsetof (ColourVertex, colour));
        colourDescriptor.vertexDescriptor.attributes[1].bufferIndex = 0;
        colourDescriptor.vertexDescriptor.layouts[0].stride = sizeof (ColourVertex);

        colourPipeline = [device newRenderPipelineStateWithDescriptor: colourDescriptor error: &error];
        [colourDescriptor release];

        [chromeVertex release];
        [chromeFragment release];
        [colourVertex release];
        [colourFragment release];

        if (chromePipeline == nil || colourPipeline == nil)
        {
            NSLog (@"[MetalHost #%d] render pipeline build failed: %@", instanceId, error);
            return;
        }

        MTLSamplerDescriptor* samplerDescriptor = [[MTLSamplerDescriptor alloc] init];
        samplerDescriptor.minFilter = MTLSamplerMinMagFilterLinear;
        samplerDescriptor.magFilter = MTLSamplerMinMagFilterLinear;
        samplerDescriptor.sAddressMode = MTLSamplerAddressModeClampToEdge;
        samplerDescriptor.tAddressMode = MTLSamplerAddressModeClampToEdge;
        chromeSampler = [device newSamplerStateWithDescriptor: samplerDescriptor];
        [samplerDescriptor release];

        const ChromeVertex quad[] = {
            { { -1.0f, -1.0f }, { 0.0f, 1.0f } },
            { {  1.0f, -1.0f }, { 1.0f, 1.0f } },
            { { -1.0f,  1.0f }, { 0.0f, 0.0f } },
            { {  1.0f, -1.0f }, { 1.0f, 1.0f } },
            { {  1.0f,  1.0f }, { 1.0f, 0.0f } },
            { { -1.0f,  1.0f }, { 0.0f, 0.0f } }
        };
        chromeQuadBuffer = [device newBufferWithBytes: quad
                                               length: sizeof (quad)
                                              options: MTLResourceStorageModeShared];

        analyzerFillSlotBytes_ = ((sizeof (ColourVertex) * kMaxAnalyzerFillVertices) + 255) & ~size_t (255);
        analyzerLineSlotBytes_ = ((sizeof (ColourVertex) * kMaxAnalyzerBins) + 255) & ~size_t (255);
        analyzerFillBuffer = [device newBufferWithLength: analyzerFillSlotBytes_ * kMaxAnalyzerTraceSlots
                                                 options: MTLResourceStorageModeShared];
        analyzerLineBuffer = [device newBufferWithLength: analyzerLineSlotBytes_ * kMaxAnalyzerTraceSlots
                                                 options: MTLResourceStorageModeShared];
        phaseFanFillBuffer = [device newBufferWithLength: sizeof (ColourVertex) * kMaxPhaseFanFillVertices
                                                 options: MTLResourceStorageModeShared];
        phaseFanLineBuffer = [device newBufferWithLength: sizeof (ColourVertex) * kMaxPhaseFanRibbonVertices
                                                 options: MTLResourceStorageModeShared];
        gonioPointBuffer = [device newBufferWithLength: sizeof (ColourVertex) * kMaxGonioPointVertices
                                               options: MTLResourceStorageModeShared];
        meterBarBuffer = [device newBufferWithLength: sizeof (ColourVertex) * kMaxMeterBarVertices
                                             options: MTLResourceStorageModeShared];
        crosshairBuffer = [device newBufferWithLength: sizeof (ColourVertex) * kMaxCrosshairVertices
                                              options: MTLResourceStorageModeShared];
        analyzerFillVertices.resize (kMaxAnalyzerFillVertices);
        analyzerLineVertices.resize (kMaxAnalyzerBins);
        analyzerCenterlinePx_.resize (kMaxAnalyzerBins);
        gonioPointVertices.resize (kMaxGonioPointVertices);
        meterBarVertices.resize (kMaxMeterBarVertices);
        analyzerSmoothedDb.fill (-200.0f);
        analyzerRmsPreviousDb.fill (-200.0f);
        analyzerRmsTargetDb.fill (-200.0f);
        analyzerSmoothedDbL.fill (-200.0f);
        analyzerRmsPreviousDbL.fill (-200.0f);
        analyzerRmsTargetDbL.fill (-200.0f);
        analyzerSmoothedDbR.fill (-200.0f);
        analyzerRmsPreviousDbR.fill (-200.0f);
        analyzerRmsTargetDbR.fill (-200.0f);
        analyzerSmoothedDbMid.fill (-200.0f);
        analyzerRmsPreviousDbMid.fill (-200.0f);
        analyzerRmsTargetDbMid.fill (-200.0f);
        analyzerSmoothedDbSide.fill (-200.0f);
        analyzerRmsPreviousDbSide.fill (-200.0f);
        analyzerRmsTargetDbSide.fill (-200.0f);
        analyzerSmoothedDbMono.fill (-200.0f);
        analyzerRmsPreviousDbMono.fill (-200.0f);
        analyzerRmsTargetDbMono.fill (-200.0f);
        analyzerRmsIntervalHistory.fill (0.0);
        analyzerPeakDb.fill (-200.0f);
        analyzerPeakHoldDb.fill (-200.0f);

        renderPipelinesReady = chromeSampler != nil
            && chromeQuadBuffer != nil
            && analyzerFillBuffer != nil
            && analyzerLineBuffer != nil
            && phaseFanFillBuffer != nil
            && phaseFanLineBuffer != nil
            && gonioPointBuffer != nil
            && meterBarBuffer != nil
            ;
    }

    double consumeRenderDeltaSeconds()
    {
        const double now = CACurrentMediaTime();
        if (lastRenderFrameTime <= 0.0)
        {
            lastRenderFrameTime = now;
            return 1.0 / 60.0;
        }

        const double elapsed = now - lastRenderFrameTime;
        lastRenderFrameTime = now;
        return juce::jlimit (1.0 / 240.0, 0.100, elapsed);
    }

    void renderFrame (id<CAMetalDrawable> drawable)
    {
        if (stopping.load (std::memory_order_acquire) || drawable == nil || commandQueue == nil)
        {
            [drawable release];
            return;
        }

        id<MTLCommandBuffer> commandBuffer = [commandQueue commandBuffer];
        if (commandBuffer == nil)
        {
            [drawable release];
            return;
        }

        auto frame = std::atomic_load_explicit (&latestChromeFrame, std::memory_order_acquire);
        updateChromeTextureIfNeeded (frame);
        id<MTLTexture> currentChromeTexture = getCurrentChromeTexture();

        const double renderDtSeconds = consumeRenderDeltaSeconds();
        const double encodeStart = CACurrentMediaTime();
        if (currentChromeTexture != nil && renderPipelinesReady)
            drawRenderPass (commandBuffer, drawable.texture, currentChromeTexture, renderDtSeconds);
        else if (currentChromeTexture != nil)
            drawChromeFallbackBlit (commandBuffer, drawable.texture, currentChromeTexture);
        else
            drawEmptyClear (commandBuffer, drawable.texture);
        const double encodeSeconds = CACurrentMediaTime() - encodeStart;
        encodeAccumulatedSeconds += encodeSeconds;
        gMetalHostEncodeMs.store (static_cast<float> (encodeSeconds * 1000.0), std::memory_order_relaxed);

        if (metalLayer.presentsWithTransaction)
        {
            // With presentsWithTransaction enabled, drawable present latches onto
            // the current CA transaction. Create and commit that transaction on the
            // render thread after the command buffer is scheduled, instead of using
            // commandBuffer presentDrawable: inside an outer transaction.
            [commandBuffer commit];
            [commandBuffer waitUntilScheduled];
            [CATransaction begin];
            [CATransaction setDisableActions: YES];
            [drawable present];
            [CATransaction commit];
            [drawable release];
        }
        else
        {
            [commandBuffer presentDrawable: drawable];
            [commandBuffer commit];
            [commandBuffer waitUntilScheduled];
            [drawable release];
        }

        gMetalHostRenderedFrames.fetch_add (1, std::memory_order_acq_rel);
        updateFps();
    }

    void drawRenderPass (id<MTLCommandBuffer> commandBuffer,
                         id<MTLTexture> drawableTexture,
                         id<MTLTexture> currentChromeTexture,
                         double renderDtSeconds)
    {
        MTLRenderPassColorAttachmentDescriptor* colour = renderPassDescriptor.colorAttachments[0];
        colour.texture = drawableTexture;
        colour.loadAction = MTLLoadActionClear;
        colour.storeAction = MTLStoreActionStore;
        colour.clearColor = MTLClearColorMake (0.02, 0.02, 0.025, 1.0);

        id<MTLRenderCommandEncoder> encoder = [commandBuffer renderCommandEncoderWithDescriptor: renderPassDescriptor];
        if (encoder == nil)
            return;

        [encoder setRenderPipelineState: chromePipeline];
        [encoder setVertexBuffer: chromeQuadBuffer offset: 0 atIndex: 0];
        [encoder setFragmentTexture: currentChromeTexture atIndex: 0];
        [encoder setFragmentSamplerState: chromeSampler atIndex: 0];
        [encoder drawPrimitives: MTLPrimitiveTypeTriangle vertexStart: 0 vertexCount: 6];

        auto analyzerFrame = std::atomic_load_explicit (&latestAnalyzerFrame, std::memory_order_acquire);
        drawAnalyzerFrame (encoder, drawableTexture, analyzerFrame, renderDtSeconds);
        drawPhaseFanFrame (encoder, drawableTexture, analyzerFrame);
        drawGonioFrame (encoder, drawableTexture, analyzerFrame);
        drawMeterBars (encoder, drawableTexture, analyzerFrame);
        [encoder endEncoding];
    }

    void drawChromeFallbackBlit (id<MTLCommandBuffer> commandBuffer,
                                 id<MTLTexture> drawableTexture,
                                 id<MTLTexture> currentChromeTexture)
    {
        id<MTLBlitCommandEncoder> blit = [commandBuffer blitCommandEncoder];
        const NSUInteger width = std::min ([currentChromeTexture width], [drawableTexture width]);
        const NSUInteger height = std::min ([currentChromeTexture height], [drawableTexture height]);
        [blit copyFromTexture: currentChromeTexture
                  sourceSlice: 0
                  sourceLevel: 0
                 sourceOrigin: MTLOriginMake (0, 0, 0)
                   sourceSize: MTLSizeMake (width, height, 1)
                    toTexture: drawableTexture
             destinationSlice: 0
             destinationLevel: 0
            destinationOrigin: MTLOriginMake (0, 0, 0)];
        [blit endEncoding];
    }

    void drawEmptyClear (id<MTLCommandBuffer> commandBuffer, id<MTLTexture> drawableTexture)
    {
        MTLRenderPassColorAttachmentDescriptor* colour = renderPassDescriptor.colorAttachments[0];
        colour.texture = drawableTexture;
        colour.loadAction = MTLLoadActionClear;
        colour.storeAction = MTLStoreActionStore;
        colour.clearColor = MTLClearColorMake (0.02, 0.02, 0.025, 1.0);

        id<MTLRenderCommandEncoder> encoder = [commandBuffer renderCommandEncoderWithDescriptor: renderPassDescriptor];
        [encoder endEncoding];
    }

    static float sanitizeAnalyzerDb (float db) noexcept
    {
        return std::isfinite (db) ? juce::jlimit (-200.0f, 24.0f, db) : -200.0f;
    }

    static void updateInterpolatedAnalyzerDb (float sourceDb,
                                              std::array<float, kMaxAnalyzerBins>& previousDb,
                                              std::array<float, kMaxAnalyzerBins>& targetDb,
                                              std::array<float, kMaxAnalyzerBins>& smoothedDb,
                                              size_t index,
                                              bool resetState,
                                              bool gotNewSnapshot,
                                              float interpolationAlpha) noexcept
    {
        const float sanitizedDb = sanitizeAnalyzerDb (sourceDb);
        if (resetState)
        {
            previousDb[index] = sanitizedDb;
            targetDb[index] = sanitizedDb;
            smoothedDb[index] = sanitizedDb;
            return;
        }

        if (gotNewSnapshot)
        {
            previousDb[index] = targetDb[index];
            targetDb[index] = sanitizedDb;
        }

        smoothedDb[index] = sanitizeAnalyzerDb (previousDb[index]
            + (targetDb[index] - previousDb[index]) * interpolationAlpha);
    }

    static float timeConstantAlpha (float timeMs, double dtSeconds) noexcept
    {
        if (timeMs <= 0.0f || dtSeconds <= 0.0)
            return 1.0f;

        const double tauSeconds = static_cast<double> (timeMs) / 1000.0;
        return static_cast<float> (juce::jlimit (0.0, 1.0, 1.0 - std::exp (-dtSeconds / tauSeconds)));
    }

    static float computeTiltDb (float freqHz, int tiltMode) noexcept
    {
        if (freqHz <= 0.0f)
            return 0.0f;

        const float octaves = std::log2 (juce::jmax (1.0f, freqHz) / 1000.0f);
        switch (tiltMode)
        {
            case 1:  return  3.0f * octaves; // Pink compensation
            case 2:  return -3.0f * octaves; // White compensation
            default: return  0.0f;
        }
    }

    static MetalColour withAlpha (MetalColour colour, float alpha) noexcept
    {
        colour.a = juce::jlimit (0.0f, 1.0f, alpha);
        return colour;
    }

    static float pixelXToNdc (float x, float width) noexcept
    {
        return (x / juce::jmax (1.0f, width)) * 2.0f - 1.0f;
    }

    static float pixelYToNdc (float y, float height) noexcept
    {
        return 1.0f - (y / juce::jmax (1.0f, height)) * 2.0f;
    }

    size_t updateAnalyzerPipelineFromSnapshot (const MetalAnalyzerFrame& frame, double renderDtSeconds)
    {
        ++analyzerPipelineCalls;

        if (analyzerEngine == nullptr || ! frame.valid)
            return 0;

        const bool gotNewSnapshot = analyzerEngine->getLatestSnapshot (renderSnapshot);
        if (gotNewSnapshot)
            ++analyzerDataChanges;
        else if (! analyzerPipelinePrimed)
            return 0;

        const int snapshotBins = (renderSnapshot.fftBinCount > 0) ? renderSnapshot.fftBinCount : renderSnapshot.numBins;
        const int expectedBins = renderSnapshot.fftSize / 2 + 1;
        if (! renderSnapshot.isValid
            || snapshotBins <= 1
            || expectedBins <= 1
            || snapshotBins != expectedBins
            || renderSnapshot.sampleRate <= 0.0
            || renderSnapshot.fftSize <= 0)
        {
            return 0;
        }

        const size_t validBins = std::min (static_cast<size_t> (snapshotBins), kMaxAnalyzerBins);
        if (validBins <= 1)
            return 0;

        const bool resetState = ! analyzerPipelinePrimed
            || analyzerPipelineBins != validBins
            || analyzerPipelineFftSize != renderSnapshot.fftSize
            || std::abs (analyzerPipelineSampleRate - renderSnapshot.sampleRate) > 0.001;

        analyzerPipelineBins = validBins;
        analyzerPipelineFftSize = renderSnapshot.fftSize;
        analyzerPipelineSampleRate = renderSnapshot.sampleRate;

        int rmsAbovePeakViolations = 0;

        const float holdReleaseMs       = juce::jmax (1.0f, frame.peakHoldDecayMs);
        const float holdDecayDbPerSec   = 60000.0f / holdReleaseMs;
        const float holdDecayThisFrame  = holdDecayDbPerSec * static_cast<float> (juce::jmax (0.0, renderDtSeconds));

        lastRenderDtSeconds = renderDtSeconds;
        lastRenderAttackMs = 0.0f;
        lastRenderReleaseMs = 0.0f;
        lastRenderAttackAlpha = 0.0f;
        lastRenderReleaseAlpha = 0.0f;

        const double now = CACurrentMediaTime();
        if (resetState)
        {
            analyzerRmsLastSnapshotTime = gotNewSnapshot ? now : 0.0;
            analyzerRmsMeasuredIntervalSeconds = 0.0;
            analyzerRmsInterpolationStartTime = now;
            analyzerRmsIntervalHistory.fill (0.0);
            analyzerRmsIntervalHistoryCount = 0;
            analyzerRmsIntervalHistoryIndex = 0;
        }
        else if (gotNewSnapshot)
        {
            if (analyzerRmsLastSnapshotTime > 0.0)
            {
                const double measuredInterval = now - analyzerRmsLastSnapshotTime;
                if (std::isfinite (measuredInterval) && measuredInterval > 0.0)
                {
                    analyzerRmsIntervalHistory[analyzerRmsIntervalHistoryIndex] =
                        juce::jlimit (kRmsInterpolationMinIntervalSeconds,
                                      kRmsInterpolationMaxIntervalSeconds,
                                      measuredInterval);
                    analyzerRmsIntervalHistoryIndex =
                        (analyzerRmsIntervalHistoryIndex + 1) % kRmsInterpolationIntervalHistorySize;
                    if (analyzerRmsIntervalHistoryCount < kRmsInterpolationIntervalHistorySize)
                        ++analyzerRmsIntervalHistoryCount;

                    double intervalSum = 0.0;
                    for (size_t i = 0; i < analyzerRmsIntervalHistoryCount; ++i)
                        intervalSum += analyzerRmsIntervalHistory[i];

                    analyzerRmsMeasuredIntervalSeconds =
                        juce::jlimit (kRmsInterpolationMinIntervalSeconds,
                                      kRmsInterpolationMaxIntervalSeconds,
                                      intervalSum / static_cast<double> (analyzerRmsIntervalHistoryCount));
                }
            }

            analyzerRmsLastSnapshotTime = now;
            analyzerRmsInterpolationStartTime = now;
        }

        const float rmsInterpolationAlpha = resetState
            ? 1.0f
            : (analyzerRmsMeasuredIntervalSeconds > 0.0
                ? static_cast<float> (juce::jlimit (0.0,
                                                    1.0,
                                                    (now - analyzerRmsInterpolationStartTime) / analyzerRmsMeasuredIntervalSeconds))
                : 1.0f);
        lastRmsInterpolationAlpha = rmsInterpolationAlpha;
        lastRmsInterpolationIntervalMs = static_cast<float> (analyzerRmsMeasuredIntervalSeconds * 1000.0);

        for (size_t i = 0; i < validBins; ++i)
        {
            // The snapshot already contains the shared engine's dB conversion, weighting, and
            // spectral smoothing. Match the CPU UI here: no second ballistics stage, only
            // linear interpolation between published snapshots to bridge data-rate gaps.
            const float targetDb = sanitizeAnalyzerDb (renderSnapshot.fftDb[i]);
            const float peakSourceDb = sanitizeAnalyzerDb (renderSnapshot.fftPeakDb[i]);
            updateInterpolatedAnalyzerDb (renderSnapshot.fftDbLRms[i],
                                          analyzerRmsPreviousDbL,
                                          analyzerRmsTargetDbL,
                                          analyzerSmoothedDbL,
                                          i,
                                          resetState,
                                          gotNewSnapshot,
                                          rmsInterpolationAlpha);
            updateInterpolatedAnalyzerDb (renderSnapshot.fftDbRRms[i],
                                          analyzerRmsPreviousDbR,
                                          analyzerRmsTargetDbR,
                                          analyzerSmoothedDbR,
                                          i,
                                          resetState,
                                          gotNewSnapshot,
                                          rmsInterpolationAlpha);
            updateInterpolatedAnalyzerDb (renderSnapshot.fftDbMidRms[i],
                                          analyzerRmsPreviousDbMid,
                                          analyzerRmsTargetDbMid,
                                          analyzerSmoothedDbMid,
                                          i,
                                          resetState,
                                          gotNewSnapshot,
                                          rmsInterpolationAlpha);
            updateInterpolatedAnalyzerDb (renderSnapshot.fftDbSideRms[i],
                                          analyzerRmsPreviousDbSide,
                                          analyzerRmsTargetDbSide,
                                          analyzerSmoothedDbSide,
                                          i,
                                          resetState,
                                          gotNewSnapshot,
                                          rmsInterpolationAlpha);
            updateInterpolatedAnalyzerDb (renderSnapshot.fftDbMonoRms[i],
                                          analyzerRmsPreviousDbMono,
                                          analyzerRmsTargetDbMono,
                                          analyzerSmoothedDbMono,
                                          i,
                                          resetState,
                                          gotNewSnapshot,
                                          rmsInterpolationAlpha);
            if (resetState)
            {
                analyzerRmsPreviousDb[i] = targetDb;
                analyzerRmsTargetDb[i] = targetDb;
                analyzerSmoothedDb[i] = targetDb;
                analyzerPeakDb[i] = sanitizeAnalyzerDb (juce::jmax (peakSourceDb, targetDb));
                analyzerPeakHoldDb[i] = analyzerPeakDb[i];
                continue;
            }

            if (gotNewSnapshot)
            {
                analyzerRmsPreviousDb[i] = analyzerRmsTargetDb[i];
                analyzerRmsTargetDb[i] = targetDb;
            }

            analyzerSmoothedDb[i] = sanitizeAnalyzerDb (analyzerRmsPreviousDb[i]
                + (analyzerRmsTargetDb[i] - analyzerRmsPreviousDb[i]) * rmsInterpolationAlpha);

            analyzerPeakDb[i] = sanitizeAnalyzerDb (juce::jmax (peakSourceDb, targetDb));

            const float rmsFloorDb = analyzerSmoothedDb[i];
            analyzerPeakDb[i] = sanitizeAnalyzerDb (juce::jmax (analyzerPeakDb[i], rmsFloorDb));

            if (renderSnapshot.isHoldOn)
            {
                // Mode 1 — infinite hold: latch the running max, never decay (Reset clears it).
                analyzerPeakHoldDb[i] = sanitizeAnalyzerDb (juce::jmax (analyzerPeakHoldDb[i], analyzerPeakDb[i]));
            }
            else
            {
                // Mode 2 — decay: held peak falls toward the live peak over the Release time.
                const float decayed = analyzerPeakHoldDb[i] - holdDecayThisFrame;
                analyzerPeakHoldDb[i] = sanitizeAnalyzerDb (juce::jmax (analyzerPeakDb[i], decayed));
            }

            if (std::isfinite (rmsFloorDb)
                && std::isfinite (analyzerPeakHoldDb[i])
                && rmsFloorDb > analyzerPeakHoldDb[i])
            {
                ++rmsAbovePeakViolations;
            }
        }

        lastRmsAbovePeakViolations = rmsAbovePeakViolations;
        analyzerPipelinePrimed = true;
        return validBins;
    }

    bool buildAnalyzerVertices (const MetalAnalyzerFrame& frame,
                                size_t validBins,
                                float drawableWidth,
                                float drawableHeight,
                                size_t& fillVertexCount,
                                size_t& lineVertexCount)
    {
        fillVertexCount = 0;
        lineVertexCount = 0;

        if (! frame.valid
            || validBins <= 1
            || frame.plotRectPx.isEmpty()
            || analyzerPipelineSampleRate <= 0.0
            || analyzerPipelineFftSize <= 0
            || frame.maxHz <= frame.minHz
            || frame.minHz <= 0.0f
            || frame.topDb <= frame.bottomDb)
        {
            return false;
        }

        const float logMin = std::log10 (frame.minHz);
        const float logMax = std::log10 (frame.maxHz);
        const float logRange = logMax - logMin;
        if (logRange <= 0.0f)
            return false;

        const size_t maxBins = std::min (validBins, kMaxAnalyzerBins);
        const float binHz = static_cast<float> (analyzerPipelineSampleRate) / static_cast<float> (analyzerPipelineFftSize);
        const float bottomY = frame.plotRectPx.y + frame.plotRectPx.h;
        const float dbRange = frame.topDb - frame.bottomDb;

        float lastX = -std::numeric_limits<float>::max();
        for (size_t i = 1; i < maxBins; ++i)
        {
            const float freqHz = static_cast<float> (i) * binHz;
            if (freqHz < frame.minHz || freqHz > frame.maxHz)
                continue;

            const float xNorm = (std::log10 (freqHz) - logMin) / logRange;
            const float mappedX = frame.plotRectPx.x + xNorm * frame.plotRectPx.w;
            const float x = (lineVertexCount == 0) ? frame.plotRectPx.x : mappedX;
            if (! std::isfinite (x) || x <= lastX)
                continue;

            const float db = juce::jlimit (frame.bottomDb, frame.topDb + 18.0f, analyzerSmoothedDb[i]);
            const float yNorm = (frame.topDb - db) / dbRange;
            const float y = frame.plotRectPx.y + yNorm * frame.plotRectPx.h;
            if (! std::isfinite (y))
                continue;

            const ColourVertex top {
                { pixelXToNdc (x, drawableWidth), pixelYToNdc (y, drawableHeight) },
                { frame.rmsColour.r, frame.rmsColour.g, frame.rmsColour.b, 0.75f }
            };
            const ColourVertex bottom {
                { pixelXToNdc (x, drawableWidth), pixelYToNdc (bottomY, drawableHeight) },
                { frame.rmsColour.r, frame.rmsColour.g, frame.rmsColour.b, 0.07f }
            };

            analyzerLineVertices[lineVertexCount++] = top;
            analyzerFillVertices[fillVertexCount++] = top;
            analyzerFillVertices[fillVertexCount++] = bottom;
            lastX = x;

            if (lineVertexCount >= kMaxAnalyzerBins || fillVertexCount + 2 > kMaxAnalyzerFillVertices)
                break;
        }

        return lineVertexCount >= 2 && fillVertexCount >= 4;
    }

    bool buildTraceVertices (const MetalAnalyzerFrame& frame,
                             const MetalTracePayload& trace,
                             float drawableWidth,
                             float drawableHeight,
                             size_t& fillVertexCount,
                             size_t& lineVertexCount)
    {
        fillVertexCount = 0;
        lineVertexCount = 0;

        if (! frame.valid
            || ! trace.visible
            || trace.db.size() <= 1
            || frame.plotRectPx.isEmpty()
            || frame.sampleRate <= 0.0
            || frame.fftSize <= 0
            || frame.maxHz <= frame.minHz
            || frame.minHz <= 0.0f
            || frame.topDb <= frame.bottomDb)
        {
            return false;
        }

        const float logMin = std::log10 (frame.minHz);
        const float logMax = std::log10 (frame.maxHz);
        const float logRange = logMax - logMin;
        if (logRange <= 0.0f)
            return false;

        const size_t maxBins = std::min (trace.db.size(), kMaxAnalyzerBins);
        const float binHz = static_cast<float> (frame.sampleRate) / static_cast<float> (frame.fftSize);
        const float bottomY = frame.plotRectPx.y + frame.plotRectPx.h;
        const float dbRange = frame.topDb - frame.bottomDb;

        float lastX = -std::numeric_limits<float>::max();
        for (size_t i = 1; i < maxBins; ++i)
        {
            const float freqHz = static_cast<float> (i) * binHz;
            if (freqHz < frame.minHz || freqHz > frame.maxHz)
                continue;

            const float xNorm = (std::log10 (freqHz) - logMin) / logRange;
            const float mappedX = frame.plotRectPx.x + xNorm * frame.plotRectPx.w;
            const float x = (lineVertexCount == 0) ? frame.plotRectPx.x : mappedX;
            if (! std::isfinite (x) || x <= lastX)
                continue;

            const float compensatedDb = trace.db[i] + frame.displayGainDb + computeTiltDb (freqHz, frame.tiltMode);
            const float db = juce::jlimit (frame.bottomDb - 24.0f, frame.topDb + 18.0f, sanitizeAnalyzerDb (compensatedDb));
            const float yNorm = (frame.topDb - db) / dbRange;
            const float y = frame.plotRectPx.y + yNorm * frame.plotRectPx.h;
            if (! std::isfinite (y))
                continue;

            const ColourVertex top {
                { pixelXToNdc (x, drawableWidth), pixelYToNdc (y, drawableHeight) },
                { trace.colour.r, trace.colour.g, trace.colour.b, trace.colour.a }
            };
            analyzerLineVertices[lineVertexCount++] = top;
            analyzerCenterlinePx_[lineVertexCount - 1] = simd_make_float2 (x, y);

            if (trace.fillToBottom)
            {
                const auto fillTop = withAlpha (trace.colour, trace.fillTopAlpha);
                const auto fillBottom = withAlpha (trace.colour, trace.fillBottomAlpha);
                analyzerFillVertices[fillVertexCount++] = {
                    { pixelXToNdc (x, drawableWidth), pixelYToNdc (y, drawableHeight) },
                    { fillTop.r, fillTop.g, fillTop.b, fillTop.a }
                };
                analyzerFillVertices[fillVertexCount++] = {
                    { pixelXToNdc (x, drawableWidth), pixelYToNdc (bottomY, drawableHeight) },
                    { fillBottom.r, fillBottom.g, fillBottom.b, fillBottom.a }
                };
            }

            lastX = x;
            if (lineVertexCount >= kMaxAnalyzerBins || fillVertexCount + 2 > kMaxAnalyzerFillVertices)
                break;
        }

        return lineVertexCount >= 2 || fillVertexCount >= 4;
    }

    template <typename DbContainer>
    bool buildTraceVerticesFromDb (const MetalAnalyzerFrame& frame,
                                   const MetalTracePayload& trace,
                                   const DbContainer& db,
                                   size_t validBins,
                                   float drawableWidth,
                                   float drawableHeight,
                                   size_t& fillVertexCount,
                                   size_t& lineVertexCount)
    {
        fillVertexCount = 0;
        lineVertexCount = 0;

        if (! frame.valid
            || ! trace.visible
            || validBins <= 1
            || frame.plotRectPx.isEmpty()
            || frame.sampleRate <= 0.0
            || frame.fftSize <= 0
            || frame.maxHz <= frame.minHz
            || frame.minHz <= 0.0f
            || frame.topDb <= frame.bottomDb)
        {
            return false;
        }

        const float logMin = std::log10 (frame.minHz);
        const float logMax = std::log10 (frame.maxHz);
        const float logRange = logMax - logMin;
        if (logRange <= 0.0f)
            return false;

        const size_t maxBins = std::min (validBins, kMaxAnalyzerBins);
        const float binHz = static_cast<float> (frame.sampleRate) / static_cast<float> (frame.fftSize);
        const float bottomY = frame.plotRectPx.y + frame.plotRectPx.h;
        const float dbRange = frame.topDb - frame.bottomDb;

        float lastX = -std::numeric_limits<float>::max();
        for (size_t i = 1; i < maxBins; ++i)
        {
            const float freqHz = static_cast<float> (i) * binHz;
            if (freqHz < frame.minHz || freqHz > frame.maxHz)
                continue;

            const float xNorm = (std::log10 (freqHz) - logMin) / logRange;
            const float mappedX = frame.plotRectPx.x + xNorm * frame.plotRectPx.w;
            const float x = (lineVertexCount == 0) ? frame.plotRectPx.x : mappedX;
            if (! std::isfinite (x) || x <= lastX)
                continue;

            const float compensatedDb = db[i] + frame.displayGainDb + computeTiltDb (freqHz, frame.tiltMode);
            const float clampedDb = juce::jlimit (frame.bottomDb - 24.0f,
                                                  frame.topDb + 18.0f,
                                                  sanitizeAnalyzerDb (compensatedDb));
            const float yNorm = (frame.topDb - clampedDb) / dbRange;
            const float y = frame.plotRectPx.y + yNorm * frame.plotRectPx.h;
            if (! std::isfinite (y))
                continue;

            const ColourVertex top {
                { pixelXToNdc (x, drawableWidth), pixelYToNdc (y, drawableHeight) },
                { trace.colour.r, trace.colour.g, trace.colour.b, trace.colour.a }
            };
            analyzerLineVertices[lineVertexCount++] = top;
            // Record pixel-space centerline so the stroke ribbon (glow + core) can
            // tessellate it. Without this the DB path leaves the centerline stale and
            // every stroke-only trace renders as degenerate (invisible) geometry.
            analyzerCenterlinePx_[lineVertexCount - 1] = simd_make_float2 (x, y);

            if (trace.fillToBottom)
            {
                const auto fillTop = withAlpha (trace.colour, trace.fillTopAlpha);
                const auto fillBottom = withAlpha (trace.colour, trace.fillBottomAlpha);
                analyzerFillVertices[fillVertexCount++] = {
                    { pixelXToNdc (x, drawableWidth), pixelYToNdc (y, drawableHeight) },
                    { fillTop.r, fillTop.g, fillTop.b, fillTop.a }
                };
                analyzerFillVertices[fillVertexCount++] = {
                    { pixelXToNdc (x, drawableWidth), pixelYToNdc (bottomY, drawableHeight) },
                    { fillBottom.r, fillBottom.g, fillBottom.b, fillBottom.a }
                };
            }

            lastX = x;
            if (lineVertexCount >= kMaxAnalyzerBins || fillVertexCount + 2 > kMaxAnalyzerFillVertices)
                break;
        }

        return lineVertexCount >= 2 || fillVertexCount >= 4;
    }

    void emitAnalyzerFillSlice (id<MTLRenderCommandEncoder> encoder, size_t fillVertexCount)
    {
        if (analyzerFillSlot_ >= kMaxAnalyzerTraceSlots)
            return;

        const size_t offset = analyzerFillSlot_ * analyzerFillSlotBytes_;
        std::memcpy (static_cast<char*> ([analyzerFillBuffer contents]) + offset,
                     analyzerFillVertices.data(),
                     sizeof (ColourVertex) * fillVertexCount);
        [encoder setVertexBuffer: analyzerFillBuffer offset: offset atIndex: 0];
        [encoder drawPrimitives: MTLPrimitiveTypeTriangleStrip
                     vertexStart: 0
                     vertexCount: static_cast<NSUInteger> (fillVertexCount)];
        ++analyzerFillSlot_;
    }

    void emitAnalyzerLineSlice (id<MTLRenderCommandEncoder> encoder, size_t lineVertexCount)
    {
        if (analyzerLineSlot_ >= kMaxAnalyzerTraceSlots)
            return;

        const size_t offset = analyzerLineSlot_ * analyzerLineSlotBytes_;
        std::memcpy (static_cast<char*> ([analyzerLineBuffer contents]) + offset,
                     analyzerLineVertices.data(),
                     sizeof (ColourVertex) * lineVertexCount);
        [encoder setVertexBuffer: analyzerLineBuffer offset: offset atIndex: 0];
        [encoder drawPrimitives: MTLPrimitiveTypeLineStrip
                     vertexStart: 0
                     vertexCount: static_cast<NSUInteger> (lineVertexCount)];
        ++analyzerLineSlot_;
    }

    // Averaged unit normal at centerline point k (perpendicular, pixel space).
    simd_float2 centerlineNormalPx (size_t k, size_t count) const
    {
        const size_t a = (k == 0) ? 0 : k - 1;
        const size_t b = (k + 1 >= count) ? count - 1 : k + 1;
        simd_float2 t = analyzerCenterlinePx_[b] - analyzerCenterlinePx_[a];
        const float len = std::sqrt (t.x * t.x + t.y * t.y);
        if (len < 1.0e-5f)
            return simd_make_float2 (0.0f, 1.0f);

        t /= len;
        return simd_make_float2 (-t.y, t.x);
    }

    // Wide, uniform-alpha halo: one strip, two rows offset +/- halfPx from center.
    void emitGlowRibbon (id<MTLRenderCommandEncoder> enc,
                         size_t count,
                         float halfPx,
                         MetalColour c,
                         float alpha,
                         float drawableWidth,
                         float drawableHeight)
    {
        if (count < 2)
            return;

        size_t n = 0;
        for (size_t k = 0; k < count; ++k)
        {
            const simd_float2 p = analyzerCenterlinePx_[k];
            const simd_float2 nrm = centerlineNormalPx (k, count);
            const simd_float2 hi = p + nrm * halfPx;
            const simd_float2 lo = p - nrm * halfPx;
            analyzerFillVertices[n++] = {
                { pixelXToNdc (hi.x, drawableWidth), pixelYToNdc (hi.y, drawableHeight) },
                { c.r, c.g, c.b, alpha }
            };
            analyzerFillVertices[n++] = {
                { pixelXToNdc (lo.x, drawableWidth), pixelYToNdc (lo.y, drawableHeight) },
                { c.r, c.g, c.b, alpha }
            };
            if (n + 2 > kMaxAnalyzerFillVertices)
                break;
        }

        emitAnalyzerFillSlice (enc, n);
    }

    bool drawTracePayload (id<MTLRenderCommandEncoder> encoder,
                           const MetalAnalyzerFrame& frame,
                           const MetalTracePayload& trace,
                           float drawableWidth,
                           float drawableHeight)
    {
        size_t fillVertexCount = 0;
        size_t lineVertexCount = 0;
        if (! buildTraceVertices (frame, trace, drawableWidth, drawableHeight, fillVertexCount, lineVertexCount))
            return false;

        bool didDraw = false;
        if (trace.fillToBottom && fillVertexCount >= 4)
        {
            emitAnalyzerFillSlice (encoder, fillVertexCount);
            didDraw = true;
        }

        if (trace.strokeVisible && lineVertexCount >= 2)
        {
            emitAnalyzerLineSlice (encoder, lineVertexCount);
            didDraw = true;
        }

        return didDraw;
    }

    template <typename DbContainer>
    bool drawTracePayloadFromDb (id<MTLRenderCommandEncoder> encoder,
                                 const MetalAnalyzerFrame& frame,
                                 const MetalTracePayload& trace,
                                 const DbContainer& db,
                                 size_t validBins,
                                 float drawableWidth,
                                 float drawableHeight)
    {
        size_t fillVertexCount = 0;
        size_t lineVertexCount = 0;
        if (! buildTraceVerticesFromDb (frame,
                                        trace,
                                        db,
                                        validBins,
                                        drawableWidth,
                                        drawableHeight,
                                        fillVertexCount,
                                        lineVertexCount))
        {
            return false;
        }

        bool didDraw = false;
        if (trace.fillToBottom && fillVertexCount >= 4)
        {
            emitAnalyzerFillSlice (encoder, fillVertexCount);
            didDraw = true;
        }

        if (trace.strokeVisible && lineVertexCount >= 2)
        {
            const float plotW      = frame.plotRectPx.w;
            const float widthScale = juce::jlimit (0.9f, 1.4f, 0.9f + 0.0015f * plotW);
            const float base       = (trace.strokeWidthPx > 0.0f ? trace.strokeWidthPx : 1.8f);
            const float coreHalf   = 0.5f * base * widthScale;
            const auto& look       = frame.look;
            const float glowHalf   = coreHalf * (trace.emphasize ? look.glowMultEmph : look.glowMultNorm);
            const float shadowHalf = coreHalf * look.shadowMult;
            const float coreAlpha  = trace.colour.a * (trace.emphasize ? look.coreAlphaEmph : look.coreAlphaNorm);
            const float glowAlpha  = trace.colour.a * (trace.emphasize ? look.glowAlphaEmph : look.glowAlphaNorm);
            const float shadowAlpha= trace.colour.a * look.shadowAlpha;

            emitGlowRibbon (encoder, lineVertexCount, shadowHalf, trace.colour, shadowAlpha, drawableWidth, drawableHeight);
            emitGlowRibbon (encoder, lineVertexCount, glowHalf,   trace.colour, glowAlpha,   drawableWidth, drawableHeight);
            emitGlowRibbon (encoder, lineVertexCount, coreHalf,   trace.colour, coreAlpha,   drawableWidth, drawableHeight);

            if (trace.emphasize)
            {
                MetalColour hi = trace.colour;
                hi.r = juce::jmin (1.0f, hi.r + look.hiBrighten);
                hi.g = juce::jmin (1.0f, hi.g + look.hiBrighten);
                hi.b = juce::jmin (1.0f, hi.b + look.hiBrighten);
                const float hiHalf  = coreHalf * look.hiMult;
                const float hiAlpha = trace.colour.a * look.hiAlpha;
                emitGlowRibbon (encoder, lineVertexCount, hiHalf, hi, hiAlpha, drawableWidth, drawableHeight);
            }

            didDraw = true;
        }

        return didDraw;
    }

    template <typename DbContainer>
    bool drawTracePayloadPipelineFirst (id<MTLRenderCommandEncoder> encoder,
                                        const MetalAnalyzerFrame& frame,
                                        const MetalTracePayload& trace,
                                        const DbContainer& db,
                                        size_t validBins,
                                        float drawableWidth,
                                        float drawableHeight)
    {
        if (! trace.visible)
            return false;

        if (validBins > 1)
            return drawTracePayloadFromDb (encoder,
                                           frame,
                                           trace,
                                           db,
                                           validBins,
                                           drawableWidth,
                                           drawableHeight);

        return drawTracePayload (encoder, frame, trace, drawableWidth, drawableHeight);
    }

    static bool hasSuppliedTrace (const MetalAnalyzerFrame& frame) noexcept
    {
        return frame.rmsTrace.visible
            || frame.peakTrace.visible
            || frame.peakHoldTrace.visible
            || frame.stereoTrace.visible
            || frame.monoTrace.visible
            || frame.leftTrace.visible
            || frame.rightTrace.visible
            || frame.midTrace.visible
            || frame.sideTrace.visible;
    }

    void updateRmsTelemetry (const MetalAnalyzerFrame& frame,
                             const char* path,
                             size_t pipelineBins,
                             bool buildOk) noexcept
    {
        lastRmsPath = path;
        lastRmsPipelineBins = pipelineBins;
        lastRmsBuildOk = buildOk;
        lastRmsPipelineSampleRate = analyzerPipelineSampleRate;
        lastRmsPipelineFftSize = analyzerPipelineFftSize;
        lastRmsFrameSampleRate = frame.sampleRate;
        lastRmsFrameFftSize = frame.fftSize;
        lastRmsTopDb = frame.topDb;
        lastRmsBottomDb = frame.bottomDb;
        lastRmsMinHz = frame.minHz;
        lastRmsMaxHz = frame.maxHz;
        lastRmsPlotW = frame.plotRectPx.w;
        lastRmsPlotH = frame.plotRectPx.h;

        float minDb = std::numeric_limits<float>::infinity();
        float maxDb = -std::numeric_limits<float>::infinity();
        const size_t binsToScan = std::min (pipelineBins, kMaxAnalyzerBins);
        for (size_t i = 0; i < binsToScan; ++i)
        {
            const float db = analyzerSmoothedDb[i];
            if (! std::isfinite (db))
                continue;

            minDb = std::min (minDb, db);
            maxDb = std::max (maxDb, db);
        }

        lastRmsSmoothedMinDb = std::isfinite (minDb) ? minDb : 0.0f;
        lastRmsSmoothedMaxDb = std::isfinite (maxDb) ? maxDb : 0.0f;
    }

    void updatePeakTelemetry (const char* peakPath,
                              const char* peakHoldPath,
                              bool peakBuildOk,
                              bool peakHoldBuildOk) noexcept
    {
        lastPeakPath = peakPath;
        lastPeakHoldPath = peakHoldPath;
        lastPeakBuildOk = peakBuildOk;
        lastPeakHoldBuildOk = peakHoldBuildOk;
    }

#if ANALYZERPRO_METAL_DIAGNOSTICS
    void appendRenderFrameDiagnostic (const MetalAnalyzerFrame& frame, bool rmsDidDraw, const char* peakPath, bool peakDidDraw)
    {
        if (wroteRenderFrameDiagnostic || ! frame.rmsTrace.visible)
            return;

        wroteRenderFrameDiagnostic = true;
        if (auto* file = std::fopen ("/tmp/analyzerpro_metal_frame_diag.txt", "ab"))
        {
            std::fprintf (file,
                          "rms_did_draw=%d\n"
                          "peak_path=%s\n"
                          "peak_did_draw=%d\n",
                          rmsDidDraw ? 1 : 0,
                          peakPath,
                          peakDidDraw ? 1 : 0);
            (void) std::fclose (file);
        }
    }
#endif

    static MetalColour brightenPhaseFanColour (MetalColour colour, float amount) noexcept
    {
        colour.r = juce::jmin (1.0f, colour.r + amount);
        colour.g = juce::jmin (1.0f, colour.g + amount);
        colour.b = juce::jmin (1.0f, colour.b + amount);
        return colour;
    }

    static bool phaseFanDrawsLines (int renderMode) noexcept
    {
        return renderMode == 1 || renderMode == 2;
    }

    simd_float2 phaseFanCenterlineNormalPx (size_t k, size_t count) const noexcept
    {
        const size_t a = (k == 0) ? 0 : k - 1;
        const size_t b = (k + 1 >= count) ? count - 1 : k + 1;
        simd_float2 t = phaseFanCenterlinePx_[b] - phaseFanCenterlinePx_[a];
        const float len = std::sqrt (t.x * t.x + t.y * t.y);
        if (len < 1.0e-5f)
            return simd_make_float2 (0.0f, 1.0f);

        t /= len;
        return simd_make_float2 (-t.y, t.x);
    }

    void emitPhaseFanRibbonStrip (id<MTLRenderCommandEncoder> encoder,
                                  size_t count,
                                  float halfPx,
                                  MetalColour colour,
                                  float alpha,
                                  float drawableWidth,
                                  float drawableHeight)
    {
        if (count < 2 || phaseFanLineBuffer == nil)
            return;

        size_t vertexCount = 0;
        for (size_t k = 0; k < count; ++k)
        {
            if (vertexCount + 2 > kMaxPhaseFanRibbonVertices)
                break;

            const simd_float2 p = phaseFanCenterlinePx_[k];
            const simd_float2 nrm = phaseFanCenterlineNormalPx (k, count);
            const simd_float2 hi = p + nrm * halfPx;
            const simd_float2 lo = p - nrm * halfPx;
            const float a = juce::jlimit (0.0f, 1.0f, colour.a * alpha);

            phaseFanRibbonVertices[vertexCount++] = {
                { pixelXToNdc (hi.x, drawableWidth), pixelYToNdc (hi.y, drawableHeight) },
                { colour.r, colour.g, colour.b, a }
            };
            phaseFanRibbonVertices[vertexCount++] = {
                { pixelXToNdc (lo.x, drawableWidth), pixelYToNdc (lo.y, drawableHeight) },
                { colour.r, colour.g, colour.b, a }
            };
        }

        if (vertexCount < 4)
            return;

        std::memcpy ([phaseFanLineBuffer contents], phaseFanRibbonVertices.data(), sizeof (ColourVertex) * vertexCount);
        [encoder setVertexBuffer: phaseFanLineBuffer offset: 0 atIndex: 0];
        [encoder drawPrimitives: MTLPrimitiveTypeTriangleStrip
                     vertexStart: 0
                     vertexCount: static_cast<NSUInteger> (vertexCount)];
    }

    void emitPhaseFanRibbonPasses (id<MTLRenderCommandEncoder> encoder,
                                   size_t count,
                                   float coreWidthPx,
                                   MetalColour colour,
                                   const MetalLookTunables& look,
                                   float drawableWidth,
                                   float drawableHeight)
    {
        if (count < 2)
            return;

        const float coreHalf = 0.5f * juce::jmax (0.5f, coreWidthPx);
        const float glowHalf = coreHalf * juce::jmax (1.0f, look.phaseFanLineGlowMult);

        emitPhaseFanRibbonStrip (encoder, count, glowHalf, colour, look.phaseFanLineGlowAlpha, drawableWidth, drawableHeight);
        emitPhaseFanRibbonStrip (encoder, count, coreHalf, colour, look.phaseFanLineCoreAlpha, drawableWidth, drawableHeight);
    }

    void drawPhaseFanRibbonFromSource (id<MTLRenderCommandEncoder> encoder,
                                       const MetalAnalyzerFrame& frame,
                                       const std::array<float, MetalAnalyzerFrame::kPhaseFanAngleBins>& source,
                                       float threshold,
                                       MetalColour colour,
                                       float coreWidthPx,
                                       float drawableWidth,
                                       float drawableHeight)
    {
        size_t segmentCount = 0;

        auto flushSegment = [&]
        {
            if (segmentCount >= 2)
                emitPhaseFanRibbonPasses (encoder, segmentCount, coreWidthPx, colour, frame.look, drawableWidth, drawableHeight);
            segmentCount = 0;
        };

        for (size_t a = 0; a < kPhaseFanAngleBins; ++a)
        {
            const float rNorm = source[a];
            if (rNorm <= threshold)
            {
                flushSegment();
                continue;
            }

            if (segmentCount >= phaseFanCenterlinePx_.size())
                break;

            const auto pt = phaseFanPointFor (frame, static_cast<int> (a), rNorm);
            phaseFanCenterlinePx_[segmentCount++] = simd_make_float2 (pt.x, pt.y);
        }

        flushSegment();
    }

    static float phaseFanRadiusToPx (const MetalAnalyzerFrame& frame, float rNorm) noexcept
    {
        const float rMin = frame.phaseFanRadiusPx * 0.02f;
        const float rMax = frame.phaseFanRadiusPx;
        return juce::jmap (juce::jlimit (0.0f, 1.0f, rNorm), 0.0f, 1.0f, rMin, rMax);
    }

    static float phaseFanThetaForAngleBin (int angleBin) noexcept
    {
        static constexpr float kPiHalf = juce::MathConstants<float>::halfPi;
        return juce::jmap (static_cast<float> (angleBin),
                           0.0f,
                           static_cast<float> (kPhaseFanAngleBins - 1),
                           -kPiHalf,
                           kPiHalf);
    }

    static juce::Point<float> phaseFanPointFor (const MetalAnalyzerFrame& frame, int angleBin, float rNorm) noexcept
    {
        const float theta = phaseFanThetaForAngleBin (angleBin);
        const float rPx = phaseFanRadiusToPx (frame, rNorm);
        return { frame.phaseFanCx + std::sin (theta) * rPx,
                 frame.phaseFanCy - std::cos (theta) * rPx };
    }

    ColourVertex makePhaseFanVertex (juce::Point<float> point,
                                     MetalColour colour,
                                     float alpha,
                                     float drawableWidth,
                                     float drawableHeight) noexcept
    {
        return {
            { pixelXToNdc (point.x, drawableWidth), pixelYToNdc (point.y, drawableHeight) },
            { colour.r, colour.g, colour.b, juce::jlimit (0.0f, 1.0f, alpha) }
        };
    }

    size_t buildPhaseFanFillVertices (const MetalAnalyzerFrame& frame,
                                      std::array<ColourVertex, kMaxPhaseFanFillVertices>& vertices,
                                      float drawableWidth,
                                      float drawableHeight)
    {
        static constexpr float kContourMinDraw = 0.003f;
        size_t vertexCount = 0;
        const juce::Point<float> centre { frame.phaseFanCx, frame.phaseFanCy };

        for (size_t a = 0; a + 1 < kPhaseFanAngleBins; ++a)
        {
            const float r0 = frame.phaseFanContourRNorm[a];
            const float r1 = frame.phaseFanContourRNorm[a + 1];
            if (r0 < kContourMinDraw && r1 < kContourMinDraw)
                continue;

            if (vertexCount + 3 > vertices.size())
                break;

            vertices[vertexCount++] = makePhaseFanVertex (centre, frame.phaseFanColour, 0.04f, drawableWidth, drawableHeight);
            vertices[vertexCount++] = makePhaseFanVertex (phaseFanPointFor (frame, static_cast<int> (a), r0),
                                                          frame.phaseFanColour,
                                                          0.42f,
                                                          drawableWidth,
                                                          drawableHeight);
            vertices[vertexCount++] = makePhaseFanVertex (phaseFanPointFor (frame, static_cast<int> (a + 1), r1),
                                                          frame.phaseFanColour,
                                                          0.42f,
                                                          drawableWidth,
                                                          drawableHeight);
        }

        return vertexCount;
    }

    void drawPhaseFanFrame (id<MTLRenderCommandEncoder> encoder,
                            id<MTLTexture> drawableTexture,
                            const std::shared_ptr<const MetalAnalyzerFrame>& frame)
    {
        if (frame == nullptr || ! frame->phaseFanValid || frame->phaseFanRadiusPx <= 0.0f)
            return;

        if (phaseFanFillBuffer == nil || phaseFanLineBuffer == nil)
            return;

        const NSUInteger drawableWidth = [drawableTexture width];
        const NSUInteger drawableHeight = [drawableTexture height];
        if (drawableWidth == 0 || drawableHeight == 0)
            return;

        const NSUInteger scissorX = static_cast<NSUInteger> (juce::jlimit (0.0f, static_cast<float> (drawableWidth), frame->phaseFanRectPx.x));
        const NSUInteger scissorY = static_cast<NSUInteger> (juce::jlimit (0.0f, static_cast<float> (drawableHeight), frame->phaseFanRectPx.y));
        const float phaseFanRightF = railClipRightPx (frame.get(),
                                                      juce::jlimit (0.0f,
                                                                    static_cast<float> (drawableWidth),
                                                                    frame->phaseFanRectPx.x + frame->phaseFanRectPx.w));
        const NSUInteger scissorRight = static_cast<NSUInteger> (phaseFanRightF);
        const NSUInteger scissorBottom = static_cast<NSUInteger> (juce::jlimit (0.0f, static_cast<float> (drawableHeight), frame->phaseFanRectPx.y + frame->phaseFanRectPx.h));
        if (scissorRight <= scissorX || scissorBottom <= scissorY)
            return;

        const MTLScissorRect scissor {
            scissorX,
            scissorY,
            scissorRight - scissorX,
            scissorBottom - scissorY
        };
        [encoder setScissorRect: scissor];
        [encoder setRenderPipelineState: colourPipeline];

        const float width = static_cast<float> (drawableWidth);
        const float height = static_cast<float> (drawableHeight);

        std::array<ColourVertex, kMaxPhaseFanFillVertices> fillVertices {};
        const size_t fillVertexCount = buildPhaseFanFillVertices (*frame, fillVertices, width, height);
        if (fillVertexCount > 0 && fillVertexCount <= kMaxPhaseFanFillVertices)
        {
            std::array<ColourVertex, kMaxPhaseFanFillVertices> glowVertices {};
            const juce::Point<float> centre { frame->phaseFanCx, frame->phaseFanCy };
            const float kGlowScale = frame->look.phaseFanGlowScale;
            for (size_t i = 0; i < fillVertexCount; ++i)
            {
                glowVertices[i] = fillVertices[i];
                const float px = (fillVertices[i].position[0] + 1.0f) * 0.5f * width;
                const float py = (1.0f - fillVertices[i].position[1]) * 0.5f * height;
                const float sx = centre.x + (px - centre.x) * kGlowScale;
                const float sy = centre.y + (py - centre.y) * kGlowScale;
                glowVertices[i].position[0] = pixelXToNdc (sx, width);
                glowVertices[i].position[1] = pixelYToNdc (sy, height);
                glowVertices[i].colour[3] = frame->look.phaseFanGlowAlpha;
            }

            std::memcpy ([phaseFanFillBuffer contents], glowVertices.data(), sizeof (ColourVertex) * fillVertexCount);
            [encoder setVertexBuffer: phaseFanFillBuffer offset: 0 atIndex: 0];
            [encoder drawPrimitives: MTLPrimitiveTypeTriangle
                         vertexStart: 0
                         vertexCount: static_cast<NSUInteger> (fillVertexCount)];

            std::memcpy ([phaseFanFillBuffer contents], fillVertices.data(), sizeof (ColourVertex) * fillVertexCount);
            [encoder setVertexBuffer: phaseFanFillBuffer offset: 0 atIndex: 0];
            [encoder drawPrimitives: MTLPrimitiveTypeTriangle
                         vertexStart: 0
                         vertexCount: static_cast<NSUInteger> (fillVertexCount)];
        }

        if (phaseFanDrawsLines (frame->phaseFanRenderMode))
        {
            static constexpr float kContourMinDraw = 0.003f;
            const auto& look = frame->look;

            drawPhaseFanRibbonFromSource (encoder,
                                          *frame,
                                          frame->phaseFanContourRNorm,
                                          kContourMinDraw,
                                          frame->phaseFanColour,
                                          look.phaseFanLineWidth,
                                          width,
                                          height);

            if (frame->phaseFanPeakHoldEnabled)
            {
                auto peakColour = brightenPhaseFanColour (frame->phaseFanColour, 0.20f);
                peakColour.a = frame->phaseFanColour.a;
                drawPhaseFanRibbonFromSource (encoder,
                                              *frame,
                                              frame->phaseFanPeakRNorm,
                                              kContourMinDraw,
                                              peakColour,
                                              look.phaseFanPeakWidth,
                                              width,
                                              height);
            }
        }

        const MTLScissorRect fullScissor { 0, 0, drawableWidth, drawableHeight };
        [encoder setScissorRect: fullScissor];
    }

    static juce::Point<float> gonioPointToDrawablePx (const MetalAnalyzerFrame& frame, MetalPoint point) noexcept
    {
        return {
            frame.gonioCx + point.x * frame.gonioHalfUsable,
            frame.gonioCy - point.y * frame.gonioHalfUsable
        };
    }

    bool appendGonioPointGlowQuad (const MetalAnalyzerFrame& frame,
                                   MetalPoint point,
                                   float drawableWidth,
                                   float drawableHeight,
                                   size_t& vertexCount) noexcept
    {
        if (! std::isfinite (point.x) || ! std::isfinite (point.y))
            return true;

        if (vertexCount + 6 > gonioPointVertices.size())
            return false;

        const auto centre = gonioPointToDrawablePx (frame, point);
        const float baseHalf = frame.gonioPointHalfSizePx > 0.0f ? frame.gonioPointHalfSizePx : 0.75f;
        const float halfSize = baseHalf * frame.look.gonioGlowMult;
        const float left = centre.x - halfSize;
        const float right = centre.x + halfSize;
        const float top = centre.y - halfSize;
        const float bottom = centre.y + halfSize;
        const auto colour = frame.gonioColour;
        const float kGlowAlpha = frame.look.gonioGlowAlpha;

        const ColourVertex quad[] = {
            { { pixelXToNdc (left,  drawableWidth), pixelYToNdc (bottom, drawableHeight) }, { colour.r, colour.g, colour.b, kGlowAlpha } },
            { { pixelXToNdc (right, drawableWidth), pixelYToNdc (bottom, drawableHeight) }, { colour.r, colour.g, colour.b, kGlowAlpha } },
            { { pixelXToNdc (left,  drawableWidth), pixelYToNdc (top,    drawableHeight) }, { colour.r, colour.g, colour.b, kGlowAlpha } },
            { { pixelXToNdc (right, drawableWidth), pixelYToNdc (bottom, drawableHeight) }, { colour.r, colour.g, colour.b, kGlowAlpha } },
            { { pixelXToNdc (right, drawableWidth), pixelYToNdc (top,    drawableHeight) }, { colour.r, colour.g, colour.b, kGlowAlpha } },
            { { pixelXToNdc (left,  drawableWidth), pixelYToNdc (top,    drawableHeight) }, { colour.r, colour.g, colour.b, kGlowAlpha } }
        };

        std::memcpy (gonioPointVertices.data() + vertexCount, quad, sizeof (quad));
        vertexCount += 6;
        return true;
    }

    bool appendGonioPointQuad (const MetalAnalyzerFrame& frame,
                               MetalPoint point,
                               float alpha,
                               float drawableWidth,
                               float drawableHeight,
                               size_t& vertexCount) noexcept
    {
        if (! std::isfinite (point.x) || ! std::isfinite (point.y))
            return true;

        if (vertexCount + 6 > gonioPointVertices.size())
            return false;

        const auto centre = gonioPointToDrawablePx (frame, point);
        const float halfSize = frame.gonioPointHalfSizePx > 0.0f ? frame.gonioPointHalfSizePx : 0.75f;
        const float left = centre.x - halfSize;
        const float right = centre.x + halfSize;
        const float top = centre.y - halfSize;
        const float bottom = centre.y + halfSize;
        const auto colour = frame.gonioColour;
        const float a = juce::jlimit (0.0f, 1.0f, alpha);

        const ColourVertex quad[] = {
            { { pixelXToNdc (left,  drawableWidth), pixelYToNdc (bottom, drawableHeight) }, { colour.r, colour.g, colour.b, a } },
            { { pixelXToNdc (right, drawableWidth), pixelYToNdc (bottom, drawableHeight) }, { colour.r, colour.g, colour.b, a } },
            { { pixelXToNdc (left,  drawableWidth), pixelYToNdc (top,    drawableHeight) }, { colour.r, colour.g, colour.b, a } },
            { { pixelXToNdc (right, drawableWidth), pixelYToNdc (bottom, drawableHeight) }, { colour.r, colour.g, colour.b, a } },
            { { pixelXToNdc (right, drawableWidth), pixelYToNdc (top,    drawableHeight) }, { colour.r, colour.g, colour.b, a } },
            { { pixelXToNdc (left,  drawableWidth), pixelYToNdc (top,    drawableHeight) }, { colour.r, colour.g, colour.b, a } }
        };

        std::memcpy (gonioPointVertices.data() + vertexCount, quad, sizeof (quad));
        vertexCount += 6;
        return true;
    }

    void drawGonioFrame (id<MTLRenderCommandEncoder> encoder,
                         id<MTLTexture> drawableTexture,
                         const std::shared_ptr<const MetalAnalyzerFrame>& frame)
    {
        if (frame == nullptr || ! frame->gonioValid || frame->gonioHalfUsable <= 0.0f)
            return;

        if (gonioPointBuffer == nil || gonioPointVertices.empty())
            return;

        const NSUInteger drawableWidth = [drawableTexture width];
        const NSUInteger drawableHeight = [drawableTexture height];
        if (drawableWidth == 0 || drawableHeight == 0)
            return;

        const NSUInteger scissorX = static_cast<NSUInteger> (juce::jlimit (0.0f, static_cast<float> (drawableWidth), frame->gonioRectPx.x));
        const NSUInteger scissorY = static_cast<NSUInteger> (juce::jlimit (0.0f, static_cast<float> (drawableHeight), frame->gonioRectPx.y));
        const float gonioRightF = railClipRightPx (frame.get(),
                                                   juce::jlimit (0.0f,
                                                                 static_cast<float> (drawableWidth),
                                                                 frame->gonioRectPx.x + frame->gonioRectPx.w));
        const NSUInteger scissorRight = static_cast<NSUInteger> (gonioRightF);
        const NSUInteger scissorBottom = static_cast<NSUInteger> (juce::jlimit (0.0f, static_cast<float> (drawableHeight), frame->gonioRectPx.y + frame->gonioRectPx.h));
        if (scissorRight <= scissorX || scissorBottom <= scissorY)
            return;

        const MTLScissorRect scissor {
            scissorX,
            scissorY,
            scissorRight - scissorX,
            scissorBottom - scissorY
        };
        [encoder setScissorRect: scissor];
        [encoder setRenderPipelineState: colourPipeline];

        const float width = static_cast<float> (drawableWidth);
        const float height = static_cast<float> (drawableHeight);
        size_t vertexCount = 0;

        const int activeFrames = juce::jlimit (0,
                                               static_cast<int> (MetalAnalyzerFrame::kGonioHistoryFrames),
                                               frame->gonioActiveHistoryFrames);
        const int pointsPerFrame = juce::jlimit (1,
                                                 static_cast<int> (MetalAnalyzerFrame::kGonioPointsPerHistoryFrame),
                                                 frame->gonioPointsPerHistoryFrame);
        const int newestFrame = juce::jlimit (-1,
                                              static_cast<int> (MetalAnalyzerFrame::kGonioHistoryFrames) - 1,
                                              frame->gonioNewestHistoryFrame);

        if (activeFrames > 0 && newestFrame >= 0)
        {
            for (int age = 0; age < activeFrames; ++age)
            {
                const int frameIndex = (newestFrame - age + static_cast<int> (MetalAnalyzerFrame::kGonioHistoryFrames))
                                     % static_cast<int> (MetalAnalyzerFrame::kGonioHistoryFrames);
                const int base = frameIndex * pointsPerFrame;
                const float newestness = activeFrames <= 1
                    ? 1.0f
                    : 1.0f - (static_cast<float> (age) / static_cast<float> (activeFrames - 1));
                const float alpha = juce::jmap (newestness, 0.08f, 0.62f);

                for (int i = 0; i < pointsPerFrame; ++i)
                {
                    const int index = base + i;
                    if (index < 0 || index >= static_cast<int> (MetalAnalyzerFrame::kGonioHistoryPointCapacity))
                        break;

                    if (! appendGonioPointQuad (*frame,
                                                frame->gonioHistoryPoints[static_cast<size_t> (index)],
                                                alpha,
                                                width,
                                                height,
                                                vertexCount))
                    {
                        break;
                    }
                }
            }
        }

        const int numPoints = juce::jlimit (0,
                                            static_cast<int> (MetalAnalyzerFrame::kGonioMaxPoints),
                                            frame->gonioNumPoints);
        for (int i = 0; i < numPoints; ++i)
        {
            if (vertexCount + 12 <= gonioPointVertices.size())
                appendGonioPointGlowQuad (*frame,
                                          frame->gonioPoints[static_cast<size_t> (i)],
                                          width,
                                          height,
                                          vertexCount);

            if (! appendGonioPointQuad (*frame,
                                        frame->gonioPoints[static_cast<size_t> (i)],
                                        0.82f,
                                        width,
                                        height,
                                        vertexCount))
            {
                break;
            }
        }

        if (frame->gonioHoldEnabled)
        {
            const int holdCount = juce::jlimit (0,
                                                static_cast<int> (MetalAnalyzerFrame::kGonioHoldBins),
                                                frame->gonioHoldCount);
            for (int i = 0; i < holdCount; ++i)
            {
                if (! appendGonioPointQuad (*frame,
                                            frame->gonioHoldPoints[static_cast<size_t> (i)],
                                            0.45f,
                                            width,
                                            height,
                                            vertexCount))
                {
                    break;
                }
            }
        }

        if (vertexCount > 0 && vertexCount <= kMaxGonioPointVertices)
        {
            std::memcpy ([gonioPointBuffer contents], gonioPointVertices.data(), sizeof (ColourVertex) * vertexCount);
            [encoder setVertexBuffer: gonioPointBuffer offset: 0 atIndex: 0];
            [encoder drawPrimitives: MTLPrimitiveTypeTriangle
                         vertexStart: 0
                         vertexCount: static_cast<NSUInteger> (vertexCount)];
        }

        const MTLScissorRect fullScissor { 0, 0, drawableWidth, drawableHeight };
        [encoder setScissorRect: fullScissor];
    }

    struct MeterBarDraw
    {
        NSUInteger vertexStart = 0;
        NSUInteger vertexCount = 0;
        MTLScissorRect scissor {};
    };

    bool appendMeterQuad (float left,
                          float top,
                          float right,
                          float bottom,
                          MetalColour colour,
                          float topAlpha,
                          float bottomAlpha,
                          float drawableWidth,
                          float drawableHeight,
                          size_t& vertexCount) noexcept
    {
        if (vertexCount + 6 > meterBarVertices.size())
            return false;

        if (! std::isfinite (left)
            || ! std::isfinite (top)
            || ! std::isfinite (right)
            || ! std::isfinite (bottom)
            || right <= left
            || bottom <= top)
        {
            return true;
        }

        const float topA = colour.a * juce::jlimit (0.0f, 1.0f, topAlpha);
        const float bottomA = colour.a * juce::jlimit (0.0f, 1.0f, bottomAlpha);
        const ColourVertex quad[] = {
            { { pixelXToNdc (left,  drawableWidth), pixelYToNdc (bottom, drawableHeight) }, { colour.r, colour.g, colour.b, bottomA } },
            { { pixelXToNdc (right, drawableWidth), pixelYToNdc (bottom, drawableHeight) }, { colour.r, colour.g, colour.b, bottomA } },
            { { pixelXToNdc (left,  drawableWidth), pixelYToNdc (top,    drawableHeight) }, { colour.r, colour.g, colour.b, topA } },
            { { pixelXToNdc (right, drawableWidth), pixelYToNdc (bottom, drawableHeight) }, { colour.r, colour.g, colour.b, bottomA } },
            { { pixelXToNdc (right, drawableWidth), pixelYToNdc (top,    drawableHeight) }, { colour.r, colour.g, colour.b, topA } },
            { { pixelXToNdc (left,  drawableWidth), pixelYToNdc (top,    drawableHeight) }, { colour.r, colour.g, colour.b, topA } }
        };

        std::memcpy (meterBarVertices.data() + vertexCount, quad, sizeof (quad));
        vertexCount += 6;
        return true;
    }

    void drawMeterBars (id<MTLRenderCommandEncoder> encoder,
                        id<MTLTexture> drawableTexture,
                        const std::shared_ptr<const MetalAnalyzerFrame>& frame)
    {
        if (frame == nullptr || frame->meterCount <= 0)
            return;

        if (meterBarBuffer == nil || meterBarVertices.empty())
            return;

        const NSUInteger drawableWidth = [drawableTexture width];
        const NSUInteger drawableHeight = [drawableTexture height];
        if (drawableWidth == 0 || drawableHeight == 0)
            return;

        const float width = static_cast<float> (drawableWidth);
        const float height = static_cast<float> (drawableHeight);
        const int meterCount = juce::jlimit (0,
                                             static_cast<int> (MetalAnalyzerFrame::kMaxMeters),
                                             frame->meterCount);

        std::array<MeterBarDraw, MetalAnalyzerFrame::kMaxMeters> draws {};
        size_t drawCount = 0;
        size_t vertexCount = 0;

        for (int i = 0; i < meterCount; ++i)
        {
            const auto& bar = frame->meters[static_cast<size_t> (i)];
            const auto& rect = bar.rectPx;
            if (! bar.valid || rect.isEmpty())
                continue;

            const float scissorLeftF = juce::jlimit (0.0f, width, std::floor (rect.x));
            const float scissorTopF = juce::jlimit (0.0f, height, std::floor (rect.y));
            const float scissorRightF = railClipRightPx (frame.get(),
                                                         juce::jlimit (0.0f, width, std::ceil (rect.x + rect.w)));
            const float scissorBottomF = juce::jlimit (0.0f, height, std::ceil (rect.y + rect.h));
            if (scissorRightF <= scissorLeftF || scissorBottomF <= scissorTopF)
                continue;

            // Expand the per-bar scissor by a margin so the glow halo can bleed
            // beyond the bar rect (otherwise it is clipped to the bar and invisible).
            const float kMeterGlowMargin = frame->look.meterGlowMargin;
            const float gScissorLeftF   = juce::jlimit (0.0f, width,  scissorLeftF   - kMeterGlowMargin);
            const float gScissorTopF    = juce::jlimit (0.0f, height, scissorTopF    - kMeterGlowMargin);
            const float gScissorRightF  = railClipRightPx (frame.get(),
                                                            juce::jlimit (0.0f, width, scissorRightF + kMeterGlowMargin));
            const float gScissorBottomF = juce::jlimit (0.0f, height, scissorBottomF + kMeterGlowMargin);
            const MTLScissorRect scissor {
                static_cast<NSUInteger> (gScissorLeftF),
                static_cast<NSUInteger> (gScissorTopF),
                static_cast<NSUInteger> (gScissorRightF - gScissorLeftF),
                static_cast<NSUInteger> (gScissorBottomF - gScissorTopF)
            };

            const size_t drawStart = vertexCount;
            const float left = rect.x;
            const float right = rect.x + rect.w;
            const float barBottom = rect.y + rect.h;
            const float mainNorm = juce::jlimit (0.0f, 1.0f, bar.mainNorm);
            const float mainH = mainNorm * rect.h;
            const float mainTop = barBottom - mainH;
            const float peakNorm = juce::jlimit (0.0f, 1.0f, bar.peakNorm);
            const float peakTop = barBottom - peakNorm * rect.h;

            if (mainH >= 0.5f)
            {
                appendMeterQuad (left - 7.0f,
                                 mainTop - 9.0f,
                                 right + 7.0f,
                                 barBottom,
                                 bar.mainColour,
                                 frame->look.meterHaloTop,
                                 frame->look.meterHaloBot,
                                 width,
                                 height,
                                 vertexCount);

                if (! appendMeterQuad (left,
                                       mainTop,
                                       right,
                                       barBottom,
                                       bar.mainColour,
                                       0.85f,
                                       0.34f,
                                       width,
                                       height,
                                       vertexCount))
                {
                    break;
                }
            }

            if (bar.displayMode == kMeterDisplayModeRms && bar.peakNorm > mainNorm)
            {
                const float capHalf = juce::jlimit (0.5f, 1.25f, rect.h * 0.0025f);
                auto peakColour = bar.peakColour;
                peakColour.a = 1.0f;

                if (mainTop > peakTop)
                {
                    if (! appendMeterQuad (left + 2.0f,
                                           peakTop,
                                           right - 2.0f,
                                           mainTop,
                                           peakColour,
                                           0.30f,
                                           0.30f,
                                           width,
                                           height,
                                           vertexCount))
                    {
                        break;
                    }
                }

                const float capTop = juce::jlimit (rect.y, barBottom, peakTop - capHalf);
                const float capBottom = juce::jlimit (rect.y, barBottom, peakTop + capHalf);

                appendMeterQuad (left + 1.0f,
                                 capTop - 2.0f,
                                 right - 1.0f,
                                 capBottom + 2.0f,
                                 peakColour,
                                 frame->look.meterCapGlowAlpha,
                                 frame->look.meterCapGlowAlpha,
                                 width,
                                 height,
                                 vertexCount);

                if (! appendMeterQuad (left + 2.0f,
                                       capTop,
                                       right - 2.0f,
                                       capBottom,
                                       peakColour,
                                       0.95f,
                                       0.95f,
                                       width,
                                       height,
                                       vertexCount))
                {
                    break;
                }
            }

            if (bar.maxPeakNorm > 0.001f)
            {
                const float maxPeakNorm = juce::jlimit (0.0f, 1.0f, bar.maxPeakNorm);
                const float maxPeakY = barBottom - maxPeakNorm * rect.h;
                auto holdColour = bar.holdColour;
                holdColour.a = 1.0f;

                if (! appendMeterQuad (left + 1.0f,
                                       juce::jlimit (rect.y, barBottom, maxPeakY - 0.75f),
                                       right - 1.0f,
                                       juce::jlimit (rect.y, barBottom, maxPeakY + 0.75f),
                                       holdColour,
                                       0.90f,
                                       0.90f,
                                       width,
                                       height,
                                       vertexCount))
                {
                    break;
                }
            }

            if (vertexCount > drawStart && drawCount < draws.size())
            {
                draws[drawCount++] = {
                    static_cast<NSUInteger> (drawStart),
                    static_cast<NSUInteger> (vertexCount - drawStart),
                    scissor
                };
            }
        }

        if (vertexCount == 0 || drawCount == 0)
            return;

        std::memcpy ([meterBarBuffer contents], meterBarVertices.data(), sizeof (ColourVertex) * vertexCount);
        [encoder setRenderPipelineState: colourPipeline];
        [encoder setVertexBuffer: meterBarBuffer offset: 0 atIndex: 0];

        for (size_t i = 0; i < drawCount; ++i)
        {
            [encoder setScissorRect: draws[i].scissor];
            [encoder drawPrimitives: MTLPrimitiveTypeTriangle
                         vertexStart: draws[i].vertexStart
                         vertexCount: draws[i].vertexCount];
        }

        const MTLScissorRect fullScissor { 0, 0, drawableWidth, drawableHeight };
        [encoder setScissorRect: fullScissor];
    }

    const std::array<float, kMaxAnalyzerBins>* crosshairDbArrayForTrace (int traceId) const noexcept
    {
        if (traceId < 0)
            return nullptr;

        using mdsp_ui::rta::TraceId;
        switch (static_cast<TraceId> (traceId))
        {
            case TraceId::Peak:   return &analyzerPeakDb;
            case TraceId::Hold:   return &analyzerPeakHoldDb;
            case TraceId::Rms:    return &analyzerSmoothedDb;
            case TraceId::Stereo: return &analyzerSmoothedDb;
            case TraceId::Left:   return &analyzerSmoothedDbL;
            case TraceId::Right:  return &analyzerSmoothedDbR;
            case TraceId::Mid:    return &analyzerSmoothedDbMid;
            case TraceId::Side:   return &analyzerSmoothedDbSide;
            case TraceId::Mono:   return &analyzerSmoothedDbMono;
            default:              return nullptr;
        }
    }

    size_t crosshairBinIndexForX (const MetalAnalyzerFrame& frame, float crosshairXPx, size_t maxBins) const noexcept
    {
        if (maxBins <= 1 || frame.plotRectPx.w <= 0.0f)
            return 0;

        const float logMin = std::log10 (frame.minHz);
        const float logMax = std::log10 (frame.maxHz);
        const float logRange = logMax - logMin;
        if (logRange <= 0.0f)
            return 0;

        const float xNorm = juce::jlimit (0.0f, 1.0f, (crosshairXPx - frame.plotRectPx.x) / frame.plotRectPx.w);
        const float freqHz = std::pow (10.0f, logMin + xNorm * logRange);
        const float binHz = static_cast<float> (frame.sampleRate) / static_cast<float> (frame.fftSize);
        if (binHz <= 0.0f)
            return 0;

        const size_t idx = static_cast<size_t> (std::lround (freqHz / binHz));
        return juce::jmin (maxBins - 1, idx);
    }

    bool appendCrosshairQuad (float left,
                              float top,
                              float right,
                              float bottom,
                              MetalColour colour,
                              float alpha,
                              float drawableWidth,
                              float drawableHeight,
                              size_t& vertexCount) noexcept
    {
        if (vertexCount + 6 > kMaxCrosshairVertices)
            return false;

        if (! std::isfinite (left) || ! std::isfinite (top) || ! std::isfinite (right) || ! std::isfinite (bottom)
            || right <= left || bottom <= top)
        {
            return true;
        }

        const float a = colour.a * juce::jlimit (0.0f, 1.0f, alpha);
        const ColourVertex quad[] = {
            { { pixelXToNdc (left,  drawableWidth), pixelYToNdc (bottom, drawableHeight) }, { colour.r, colour.g, colour.b, a } },
            { { pixelXToNdc (right, drawableWidth), pixelYToNdc (bottom, drawableHeight) }, { colour.r, colour.g, colour.b, a } },
            { { pixelXToNdc (left,  drawableWidth), pixelYToNdc (top,    drawableHeight) }, { colour.r, colour.g, colour.b, a } },
            { { pixelXToNdc (right, drawableWidth), pixelYToNdc (bottom, drawableHeight) }, { colour.r, colour.g, colour.b, a } },
            { { pixelXToNdc (right, drawableWidth), pixelYToNdc (top,    drawableHeight) }, { colour.r, colour.g, colour.b, a } },
            { { pixelXToNdc (left,  drawableWidth), pixelYToNdc (top,    drawableHeight) }, { colour.r, colour.g, colour.b, a } }
        };

        std::memcpy (crosshairVertices.data() + vertexCount, quad, sizeof (quad));
        vertexCount += 6;
        return true;
    }

    void drawCrosshair (id<MTLRenderCommandEncoder> encoder,
                        id<MTLTexture> drawableTexture,
                        const MetalAnalyzerFrame& frame,
                        size_t pipelineBins)
    {
        if (! frame.crosshairActive || crosshairBuffer == nil)
            return;

        const float width = static_cast<float> ([drawableTexture width]);
        const float height = static_cast<float> ([drawableTexture height]);
        if (width <= 0.0f || height <= 0.0f || frame.plotRectPx.isEmpty())
            return;

        const float plotRightF = railClipRightPx (&frame,
                                                  juce::jlimit (0.0f, width, frame.plotRectPx.x + frame.plotRectPx.w));
        if (frame.crosshairXPx >= plotRightF)
            return;

        const float x = juce::jlimit (frame.plotRectPx.x, plotRightF, frame.crosshairXPx);
        const float plotTop = frame.plotRectPx.y;
        const float plotBottom = frame.plotRectPx.y + frame.plotRectPx.h;

        size_t vertexCount = 0;
        auto colour = frame.crosshairColour;
        if (colour.a <= 0.0f)
            colour = { 0.9f, 0.9f, 0.9f, 0.45f };

        appendCrosshairQuad (x - 0.75f, plotTop, x + 0.75f, plotBottom, colour, colour.a, width, height, vertexCount);

        const size_t maxBins = pipelineBins > 1 ? pipelineBins : 0;
        if (frame.crosshairTraceId >= 0 && maxBins > 1)
        {
            const auto* dbArray = crosshairDbArrayForTrace (frame.crosshairTraceId);
            if (dbArray != nullptr)
            {
                const size_t binIdx = crosshairBinIndexForX (frame, x, maxBins);
                const float rawDb = (*dbArray)[binIdx];
                if (std::isfinite (rawDb) && rawDb > -199.0f)
                {
                    const float logMin = std::log10 (frame.minHz);
                    const float logMax = std::log10 (frame.maxHz);
                    const float logRange = logMax - logMin;
                    const float xNorm = juce::jlimit (0.0f, 1.0f, (x - frame.plotRectPx.x) / frame.plotRectPx.w);
                    const float freqHz = std::pow (10.0f, logMin + xNorm * logRange);
                    const float compensatedDb = rawDb + frame.displayGainDb + computeTiltDb (freqHz, frame.tiltMode);
                    const float dbRange = frame.topDb - frame.bottomDb;
                    if (dbRange > 0.0f)
                    {
                        const float clampedDb = juce::jlimit (frame.bottomDb - 24.0f,
                                                              frame.topDb + 18.0f,
                                                              sanitizeAnalyzerDb (compensatedDb));
                        const float yNorm = (frame.topDb - clampedDb) / dbRange;
                        const float y = plotTop + yNorm * frame.plotRectPx.h;
                        if (std::isfinite (y))
                        {
                            // Full-width horizontal line at the trace value (a proper crosshair,
                            // not just a short tick) so both axes read clearly.
                            appendCrosshairQuad (frame.plotRectPx.x, y - 0.75f, plotRightF, y + 0.75f, colour, 0.55f, width, height, vertexCount);
                            constexpr float kDotHalf = 3.0f;
                            appendCrosshairQuad (x - kDotHalf, y - kDotHalf, x + kDotHalf, y + kDotHalf, colour, 1.0f, width, height, vertexCount);
                        }
                    }
                }
            }
        }

        if (vertexCount == 0)
            return;

        const NSUInteger scissorX = static_cast<NSUInteger> (juce::jlimit (0.0f, width, frame.plotRectPx.x));
        const NSUInteger scissorY = static_cast<NSUInteger> (juce::jlimit (0.0f, height, frame.plotRectPx.y));
        const NSUInteger scissorRight = static_cast<NSUInteger> (plotRightF);
        const NSUInteger scissorBottom = static_cast<NSUInteger> (juce::jlimit (0.0f, height, plotBottom));
        if (scissorRight <= scissorX || scissorBottom <= scissorY)
            return;

        const MTLScissorRect scissor {
            scissorX,
            scissorY,
            scissorRight - scissorX,
            scissorBottom - scissorY
        };

        std::memcpy ([crosshairBuffer contents], crosshairVertices.data(), sizeof (ColourVertex) * vertexCount);
        [encoder setScissorRect: scissor];
        [encoder setRenderPipelineState: colourPipeline];
        [encoder setVertexBuffer: crosshairBuffer offset: 0 atIndex: 0];
        [encoder drawPrimitives: MTLPrimitiveTypeTriangle
                     vertexStart: 0
                     vertexCount: static_cast<NSUInteger> (vertexCount)];
    }

    void drawAnalyzerFrame (id<MTLRenderCommandEncoder> encoder,
                            id<MTLTexture> drawableTexture,
                            const std::shared_ptr<const MetalAnalyzerFrame>& frame,
                            double renderDtSeconds)
    {
        if (frame == nullptr)
            return;

        const size_t analyzerPipelineBinsForFrame = updateAnalyzerPipelineFromSnapshot (*frame, renderDtSeconds);
        const NSUInteger drawableWidth = [drawableTexture width];
        const NSUInteger drawableHeight = [drawableTexture height];
        const NSUInteger scissorX = static_cast<NSUInteger> (juce::jlimit (0.0f, static_cast<float> (drawableWidth), frame->plotRectPx.x));
        const NSUInteger scissorY = static_cast<NSUInteger> (juce::jlimit (0.0f, static_cast<float> (drawableHeight), frame->plotRectPx.y));
        const float plotRightF = railClipRightPx (frame.get(),
                                                  juce::jlimit (0.0f,
                                                                static_cast<float> (drawableWidth),
                                                                frame->plotRectPx.x + frame->plotRectPx.w));
        const NSUInteger scissorRight = static_cast<NSUInteger> (plotRightF);
        const NSUInteger scissorBottom = static_cast<NSUInteger> (juce::jlimit (0.0f, static_cast<float> (drawableHeight), frame->plotRectPx.y + frame->plotRectPx.h));

        if (scissorRight <= scissorX || scissorBottom <= scissorY)
            return;

        const MTLScissorRect scissor {
            scissorX,
            scissorY,
            scissorRight - scissorX,
            scissorBottom - scissorY
        };
        [encoder setScissorRect: scissor];
        [encoder setRenderPipelineState: colourPipeline];

        if (hasSuppliedTrace (*frame))
        {
            analyzerLineSlot_ = 0;
            analyzerFillSlot_ = 0;
            const float width = static_cast<float> (drawableWidth);
            const float height = static_cast<float> (drawableHeight);
            bool didDrawAnyTrace = false;

            const bool sideDrew = drawTracePayloadPipelineFirst (encoder, *frame, frame->sideTrace,
                                      analyzerSmoothedDbSide, analyzerPipelineBinsForFrame, width, height);
            const bool midDrew = drawTracePayloadPipelineFirst (encoder, *frame, frame->midTrace,
                                      analyzerSmoothedDbMid, analyzerPipelineBinsForFrame, width, height);
            const bool leftDrew = drawTracePayloadPipelineFirst (encoder, *frame, frame->leftTrace,
                                      analyzerSmoothedDbL, analyzerPipelineBinsForFrame, width, height);
            const bool rightDrew = drawTracePayloadPipelineFirst (encoder, *frame, frame->rightTrace,
                                      analyzerSmoothedDbR, analyzerPipelineBinsForFrame, width, height);
            const bool monoDrew = drawTracePayloadPipelineFirst (encoder, *frame, frame->monoTrace,
                                      analyzerSmoothedDbMono, analyzerPipelineBinsForFrame, width, height);
            const bool stereoDrew = drawTracePayloadPipelineFirst (encoder, *frame, frame->stereoTrace,
                                      analyzerSmoothedDb, analyzerPipelineBinsForFrame, width, height);
            didDrawAnyTrace |= sideDrew | midDrew | leftDrew | rightDrew | monoDrew | stereoDrew;

            // NOTE: RMS is drawn LAST (below) so it sits in FRONT of Peak/Peak-hold —
            // the peaks still read above the RMS line since they're higher in dB.
            bool rmsBuildOk = false;
            const char* rmsPath = "none";

            bool peakBuildOk = false;
            const char* peakPath = "none";
            if (frame->peakTrace.visible)
            {
                if (analyzerPipelineBinsForFrame > 1)
                {
                    peakPath = "pipeline";
                    peakBuildOk = drawTracePayloadFromDb (encoder,
                                                          *frame,
                                                          frame->peakTrace,
                                                          analyzerPeakDb,
                                                          analyzerPipelineBinsForFrame,
                                                          width,
                                                          height);
                }
                else
                {
                    peakPath = "fallback";
                    peakBuildOk = drawTracePayload (encoder, *frame, frame->peakTrace, width, height);
                }

                didDrawAnyTrace |= peakBuildOk;
            }

            bool peakHoldBuildOk = false;
            const char* peakHoldPath = "none";
            if (frame->peakHoldTrace.visible)
            {
                if (analyzerPipelineBinsForFrame > 1)
                {
                    peakHoldPath = "pipeline";
                    peakHoldBuildOk = drawTracePayloadFromDb (encoder,
                                                              *frame,
                                                              frame->peakHoldTrace,
                                                              analyzerPeakHoldDb,
                                                              analyzerPipelineBinsForFrame,
                                                              width,
                                                              height);
                }
                else
                {
                    peakHoldPath = "fallback";
                    peakHoldBuildOk = drawTracePayload (encoder, *frame, frame->peakHoldTrace, width, height);
                }

                didDrawAnyTrace |= peakHoldBuildOk;
            }

            // RMS drawn LAST → in front of Peak/Peak-hold (peaks remain visible above it).
            if (frame->rmsTrace.visible)
            {
                if (analyzerPipelineBinsForFrame > 1)
                {
                    rmsPath = "pipeline";
                    rmsBuildOk = drawTracePayloadFromDb (encoder,
                                                         *frame,
                                                         frame->rmsTrace,
                                                         analyzerSmoothedDb,
                                                         analyzerPipelineBinsForFrame,
                                                         width,
                                                         height);
                }
                else
                {
                    rmsPath = "fallback";
                    rmsBuildOk = drawTracePayload (encoder, *frame, frame->rmsTrace, width, height);
                }

                didDrawAnyTrace |= rmsBuildOk;
            }
            updateRmsTelemetry (*frame, rmsPath, analyzerPipelineBinsForFrame, rmsBuildOk);
            updatePeakTelemetry (peakPath, peakHoldPath, peakBuildOk, peakHoldBuildOk);
#if ANALYZERPRO_METAL_DIAGNOSTICS
            appendRenderFrameDiagnostic (*frame, rmsBuildOk, peakPath, peakBuildOk);
#endif

            if (! didDrawAnyTrace)
            {
                drawCrosshair (encoder, drawableTexture, *frame, analyzerPipelineBinsForFrame);
                const MTLScissorRect fullScissor { 0, 0, drawableWidth, drawableHeight };
                [encoder setScissorRect: fullScissor];
                return;
            }

            drawCrosshair (encoder, drawableTexture, *frame, analyzerPipelineBinsForFrame);
            const MTLScissorRect fullScissor { 0, 0, drawableWidth, drawableHeight };
            [encoder setScissorRect: fullScissor];
            return;
        }

        updateRmsTelemetry (*frame, "none", analyzerPipelineBinsForFrame, false);
        updatePeakTelemetry ("none", "none", false, false);
        drawCrosshair (encoder, drawableTexture, *frame, analyzerPipelineBinsForFrame);
        const MTLScissorRect fullScissor { 0, 0, drawableWidth, drawableHeight };
        [encoder setScissorRect: fullScissor];
    }

    void updateChromeTextureIfNeeded (const std::shared_ptr<const FrameTexturePayload>& frame)
    {
        if (frame == nullptr
            || frame->widthPx <= 0
            || frame->heightPx <= 0
            || frame->bytesPerRow <= 0
            || frame->bgraPixels.empty())
        {
            return;
        }

        if (uploadedChromeSequence == frame->sequence)
            return;

        if (! ensureChromeTextureRing (frame->widthPx, frame->heightPx))
            return;

        const size_t uploadSlot = nextChromeTextureUploadSlot;
        nextChromeTextureUploadSlot = (nextChromeTextureUploadSlot + 1) % chromeTextures.size();
        id<MTLTexture> targetTexture = chromeTextures[uploadSlot];
        if (targetTexture == nil)
            return;

        const MTLRegion region = MTLRegionMake2D (0, 0,
                                                  static_cast<NSUInteger> (frame->widthPx),
                                                  static_cast<NSUInteger> (frame->heightPx));
        [targetTexture replaceRegion: region
                          mipmapLevel: 0
                            withBytes: frame->bgraPixels.data()
                          bytesPerRow: static_cast<NSUInteger> (frame->bytesPerRow)];

        uploadedChromeSequence = frame->sequence;
        currentChromeTextureIndex.store (static_cast<int> (uploadSlot), std::memory_order_release);
    }

    bool ensureChromeTextureRing (int widthPx, int heightPx)
    {
        if (device == nil || widthPx <= 0 || heightPx <= 0)
            return false;

        bool ringReady = chromeTextureWidth == widthPx && chromeTextureHeight == heightPx;
        for (auto texture : chromeTextures)
            ringReady = ringReady && texture != nil;
        if (ringReady)
            return true;

        releaseChromeTextureRing();

        MTLTextureDescriptor* descriptor = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat: MTLPixelFormatBGRA8Unorm
                                                                                              width: static_cast<NSUInteger> (widthPx)
                                                                                             height: static_cast<NSUInteger> (heightPx)
                                                                                          mipmapped: NO];
        descriptor.storageMode = MTLStorageModeShared;
        descriptor.usage = MTLTextureUsageShaderRead;

        bool createdAll = true;
        for (auto& texture : chromeTextures)
        {
            texture = [device newTextureWithDescriptor: descriptor];
            createdAll = createdAll && texture != nil;
        }

        if (! createdAll)
        {
            releaseChromeTextureRing();
            return false;
        }

        chromeTextureWidth = widthPx;
        chromeTextureHeight = heightPx;
        nextChromeTextureUploadSlot = 0;
        uploadedChromeSequence = 0;
        currentChromeTextureIndex.store (kNoChromeTextureIndex, std::memory_order_release);
        return true;
    }

    void releaseChromeTextureRing()
    {
        for (auto& texture : chromeTextures)
        {
            if (texture != nil)
            {
                [texture release];
                texture = nil;
            }
        }

        chromeTextureWidth = 0;
        chromeTextureHeight = 0;
        uploadedChromeSequence = 0;
        nextChromeTextureUploadSlot = 0;
        currentChromeTextureIndex.store (kNoChromeTextureIndex, std::memory_order_release);
    }

    id<MTLTexture> getCurrentChromeTexture() const noexcept
    {
        const int index = currentChromeTextureIndex.load (std::memory_order_acquire);
        if (index < 0 || index >= static_cast<int> (chromeTextures.size()))
            return nil;

        return chromeTextures[static_cast<size_t> (index)];
    }

    void updateFps()
    {
        const double now = CACurrentMediaTime();

        if (lastFpsTime <= 0.0)
        {
            lastFpsTime = now;
            fpsFrames = 0;
            nextDrawableBlockedSeconds = 0.0;
            encodeAccumulatedSeconds = 0.0;
            analyzerPipelineCalls = 0;
            analyzerDataChanges = 0;
            return;
        }

        ++fpsFrames;
        const double elapsed = now - lastFpsTime;

        if (elapsed >= 1.0)
        {
            const float computedFps = static_cast<float> (static_cast<double> (fpsFrames) / elapsed);
            const double nextDrawableBlockPct = juce::jlimit (0.0, 1.0, nextDrawableBlockedSeconds / elapsed);
            const float averageEncodeMs = fpsFrames > 0
                ? static_cast<float> ((encodeAccumulatedSeconds / static_cast<double> (fpsFrames)) * 1000.0)
                : 0.0f;
            const float pipelineFps = static_cast<float> (static_cast<double> (analyzerPipelineCalls) / elapsed);
            const float dataFps = static_cast<float> (static_cast<double> (analyzerDataChanges) / elapsed);
            gMetalHostFps.store (computedFps, std::memory_order_relaxed);
            gMetalHostEncodeMs.store (averageEncodeMs, std::memory_order_relaxed);
#if ANALYZERPRO_METAL_DIAGNOSTICS
            writeStatsFile (computedFps, nextDrawableBlockPct, averageEncodeMs, pipelineFps, dataFps);
#endif
            NSLog (@"[MetalHost #%d] driver=self_paced_nextDrawable thread=%s mech=%s fps=%.1f nextDrawable_block_pct=%.3f encode_ms=%.3f pipeline_fps=%.1f data_fps=%.1f",
                   instanceId,
                   [NSThread isMainThread] ? "main" : "off-main",
                   getMetalHostMechanismName (mechanism),
                   static_cast<double> (computedFps),
                   nextDrawableBlockPct,
                   static_cast<double> (averageEncodeMs),
                   static_cast<double> (pipelineFps),
                   static_cast<double> (dataFps));
            fpsFrames = 0;
            lastFpsTime = now;
            nextDrawableBlockedSeconds = 0.0;
            encodeAccumulatedSeconds = 0.0;
            analyzerPipelineCalls = 0;
            analyzerDataChanges = 0;
        }
    }

    void resetStatsCounters() noexcept
    {
        lastFpsTime = 0.0;
        fpsFrames = 0;
        nextDrawableBlockedSeconds = 0.0;
        encodeAccumulatedSeconds = 0.0;
        analyzerPipelineCalls = 0;
        analyzerDataChanges = 0;
    }

#if ANALYZERPRO_METAL_DIAGNOSTICS
    void writeStatsFile (float presentFps, double nextDrawableBlockPct, float encodeMs, float pipelineFps, float dataFps) const
    {
        char tempPath[128] {};
        (void) std::snprintf (tempPath, sizeof (tempPath), "%s.%d.tmp", kMetalHostStatsPath, instanceId);

        if (auto* file = std::fopen (tempPath, "wb"))
        {
            char buffer[4096] {};
            const int bytes = std::snprintf (buffer,
                                             sizeof (buffer),
                                             "present_fps=%.2f\n"
                                             "nextDrawable_block_pct=%.4f\n"
                                             "encode_ms=%.3f\n"
                                             "pipeline_fps=%.2f\n"
                                             "data_fps=%.2f\n"
                                             "rms_path=%s\n"
                                             "rms_pipeline_bins=%zu\n"
                                             "rms_build_ok=%d\n"
                                             "rms_smoothed_min=%.2f\n"
                                             "rms_smoothed_max=%.2f\n"
                                             "peak_path=%s\n"
                                             "peak_build_ok=%d\n"
                                             "peak_hold_path=%s\n"
                                             "peak_hold_build_ok=%d\n"
                                             "rms_above_peak_violations=%d\n"
                                             "render_dt_ms=%.3f\n"
                                             "render_attack_ms=%.2f\n"
                                             "render_release_ms=%.2f\n"
                                             "render_attack_alpha=%.6f\n"
                                             "render_release_alpha=%.6f\n"
                                             "rms_interp_alpha=%.6f\n"
                                             "rms_interp_interval_ms=%.3f\n"
                                             "pipeline_sr=%.1f\n"
                                             "pipeline_fft=%d\n"
                                             "frame_sr=%.1f\n"
                                             "frame_fft=%d\n"
                                             "topDb=%.2f\n"
                                             "bottomDb=%.2f\n"
                                             "minHz=%.2f\n"
                                             "maxHz=%.2f\n"
                                             "plotW=%.1f\n"
                                             "plotH=%.1f\n"
                                             "instance_id=%d\n"
                                             "mechanism=%s\n"
                                             "live_render_threads=%d\n",
                                             static_cast<double> (presentFps),
                                             nextDrawableBlockPct,
                                             static_cast<double> (encodeMs),
                                             static_cast<double> (pipelineFps),
                                             static_cast<double> (dataFps),
                                             lastRmsPath,
                                             lastRmsPipelineBins,
                                             lastRmsBuildOk ? 1 : 0,
                                             static_cast<double> (lastRmsSmoothedMinDb),
                                             static_cast<double> (lastRmsSmoothedMaxDb),
                                             lastPeakPath,
                                             lastPeakBuildOk ? 1 : 0,
                                             lastPeakHoldPath,
                                             lastPeakHoldBuildOk ? 1 : 0,
                                             lastRmsAbovePeakViolations,
                                             lastRenderDtSeconds * 1000.0,
                                             static_cast<double> (lastRenderAttackMs),
                                             static_cast<double> (lastRenderReleaseMs),
                                             static_cast<double> (lastRenderAttackAlpha),
                                             static_cast<double> (lastRenderReleaseAlpha),
                                             static_cast<double> (lastRmsInterpolationAlpha),
                                             static_cast<double> (lastRmsInterpolationIntervalMs),
                                             lastRmsPipelineSampleRate,
                                             lastRmsPipelineFftSize,
                                             lastRmsFrameSampleRate,
                                             lastRmsFrameFftSize,
                                             static_cast<double> (lastRmsTopDb),
                                             static_cast<double> (lastRmsBottomDb),
                                             static_cast<double> (lastRmsMinHz),
                                             static_cast<double> (lastRmsMaxHz),
                                             static_cast<double> (lastRmsPlotW),
                                             static_cast<double> (lastRmsPlotH),
                                             instanceId,
                                             getMetalHostMechanismName (mechanism),
                                             liveRenderThreadCount().load (std::memory_order_relaxed));

            const bool wrote = bytes > 0
                && bytes < static_cast<int> (sizeof (buffer))
                && std::fwrite (buffer, 1, static_cast<size_t> (bytes), file) == static_cast<size_t> (bytes);
            const bool closed = std::fclose (file) == 0;

            if (wrote && closed)
                (void) std::rename (tempPath, kMetalHostStatsPath);
            else
                (void) std::remove (tempPath);
        }
    }
#endif

    juce::Component& editor;
    const AnalyzerEngine* analyzerEngine = nullptr;
    MetalHostMechanism mechanism = MetalHostMechanism::BackingLayer;
    id<MTLDevice> device = nil;
    id<MTLCommandQueue> commandQueue = nil;
    CAMetalLayer* metalLayer = nil;
    MTLRenderPassDescriptor* renderPassDescriptor = nil;
    id<MTLLibrary> shaderLibrary = nil;
    id<MTLRenderPipelineState> chromePipeline = nil;
    id<MTLRenderPipelineState> colourPipeline = nil;
    id<MTLSamplerState> chromeSampler = nil;
    id<MTLBuffer> chromeQuadBuffer = nil;
    id<MTLBuffer> analyzerFillBuffer = nil;
    id<MTLBuffer> analyzerLineBuffer = nil;
    size_t analyzerLineSlotBytes_ = 0;
    size_t analyzerFillSlotBytes_ = 0;
    size_t analyzerLineSlot_ = 0;
    size_t analyzerFillSlot_ = 0;
    id<MTLBuffer> phaseFanFillBuffer = nil;
    id<MTLBuffer> phaseFanLineBuffer = nil;
    std::array<simd_float2, kPhaseFanAngleBins> phaseFanCenterlinePx_ {};
    std::array<ColourVertex, kMaxPhaseFanRibbonVertices> phaseFanRibbonVertices {};
    id<MTLBuffer> gonioPointBuffer = nil;
    id<MTLBuffer> meterBarBuffer = nil;
    id<MTLBuffer> crosshairBuffer = nil;
    std::array<ColourVertex, kMaxCrosshairVertices> crosshairVertices {};
    std::array<id<MTLTexture>, kChromeTextureRingSize> chromeTextures {};
    int chromeTextureWidth = 0;
    int chromeTextureHeight = 0;
    uint64_t uploadedChromeSequence = 0;
    size_t nextChromeTextureUploadSlot = 0;
    std::atomic<int> currentChromeTextureIndex { kNoChromeTextureIndex };
    std::thread renderThread;
    std::atomic<bool> renderThreadExited { true };
    std::atomic<bool> renderThreadShouldExit { true };
    NSView* peerView = nil;
    CALayer* originalLayer = nil;
    AnalyzerProMetalCoverView* coverView = nil;
    BOOL originalWantsLayer = NO;
    bool layerAttached = false;
    bool initialised = false;
    bool running = false;
    bool teardownAbandoned = false;
    std::atomic<bool> stopping { false };
    std::atomic<int> inFlightFrames { 0 };
    std::atomic<float> backingScale { 1.0f };
    std::shared_ptr<const FrameTexturePayload> latestChromeFrame;
    std::shared_ptr<const MetalAnalyzerFrame> latestAnalyzerFrame;
    AnalyzerSnapshot renderSnapshot;
    std::array<float, kMaxAnalyzerBins> analyzerSmoothedDb {};
    std::array<float, kMaxAnalyzerBins> analyzerRmsPreviousDb {};
    std::array<float, kMaxAnalyzerBins> analyzerRmsTargetDb {};
    std::array<float, kMaxAnalyzerBins> analyzerSmoothedDbL {};
    std::array<float, kMaxAnalyzerBins> analyzerRmsPreviousDbL {};
    std::array<float, kMaxAnalyzerBins> analyzerRmsTargetDbL {};
    std::array<float, kMaxAnalyzerBins> analyzerSmoothedDbR {};
    std::array<float, kMaxAnalyzerBins> analyzerRmsPreviousDbR {};
    std::array<float, kMaxAnalyzerBins> analyzerRmsTargetDbR {};
    std::array<float, kMaxAnalyzerBins> analyzerSmoothedDbMid {};
    std::array<float, kMaxAnalyzerBins> analyzerRmsPreviousDbMid {};
    std::array<float, kMaxAnalyzerBins> analyzerRmsTargetDbMid {};
    std::array<float, kMaxAnalyzerBins> analyzerSmoothedDbSide {};
    std::array<float, kMaxAnalyzerBins> analyzerRmsPreviousDbSide {};
    std::array<float, kMaxAnalyzerBins> analyzerRmsTargetDbSide {};
    std::array<float, kMaxAnalyzerBins> analyzerSmoothedDbMono {};
    std::array<float, kMaxAnalyzerBins> analyzerRmsPreviousDbMono {};
    std::array<float, kMaxAnalyzerBins> analyzerRmsTargetDbMono {};
    std::array<float, kMaxAnalyzerBins> analyzerPeakDb {};
    std::array<float, kMaxAnalyzerBins> analyzerPeakHoldDb {};
    std::vector<ColourVertex> analyzerFillVertices;
    std::vector<ColourVertex> analyzerLineVertices;
    std::vector<simd_float2> analyzerCenterlinePx_;
    std::vector<ColourVertex> gonioPointVertices;
    std::vector<ColourVertex> meterBarVertices;
    double analyzerPipelineSampleRate = 0.0;
    int analyzerPipelineFftSize = 0;
    size_t analyzerPipelineBins = 0;
    bool analyzerPipelinePrimed = false;
    double lastFpsTime = 0.0;
    double lastRenderFrameTime = 0.0;
    double nextDrawableBlockedSeconds = 0.0;
    double encodeAccumulatedSeconds = 0.0;
    int fpsFrames = 0;
    int analyzerPipelineCalls = 0;
    int analyzerDataChanges = 0;
    int instanceId = 0;
    bool renderPipelinesReady = false;
#if ANALYZERPRO_METAL_DIAGNOSTICS
    bool wroteRenderFrameDiagnostic = false;
#endif
    const char* lastRmsPath = "none";
    size_t lastRmsPipelineBins = 0;
    bool lastRmsBuildOk = false;
    const char* lastPeakPath = "none";
    const char* lastPeakHoldPath = "none";
    bool lastPeakBuildOk = false;
    bool lastPeakHoldBuildOk = false;
    int lastRmsAbovePeakViolations = 0;
    double lastRenderDtSeconds = 0.0;
    float lastRenderAttackMs = 0.0f;
    float lastRenderReleaseMs = 0.0f;
    float lastRenderAttackAlpha = 0.0f;
    float lastRenderReleaseAlpha = 0.0f;
    float lastRmsInterpolationAlpha = 0.0f;
    float lastRmsInterpolationIntervalMs = 0.0f;
    double analyzerRmsInterpolationStartTime = 0.0;
    double analyzerRmsLastSnapshotTime = 0.0;
    double analyzerRmsMeasuredIntervalSeconds = 0.0;
    std::array<double, kRmsInterpolationIntervalHistorySize> analyzerRmsIntervalHistory {};
    size_t analyzerRmsIntervalHistoryCount = 0;
    size_t analyzerRmsIntervalHistoryIndex = 0;
    float lastRmsSmoothedMinDb = 0.0f;
    float lastRmsSmoothedMaxDb = 0.0f;
    double lastRmsPipelineSampleRate = 0.0;
    int lastRmsPipelineFftSize = 0;
    double lastRmsFrameSampleRate = 0.0;
    int lastRmsFrameFftSize = 0;
    float lastRmsTopDb = 0.0f;
    float lastRmsBottomDb = 0.0f;
    float lastRmsMinHz = 0.0f;
    float lastRmsMaxHz = 0.0f;
    float lastRmsPlotW = 0.0f;
    float lastRmsPlotH = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MetalHostImpl)
};

} // namespace AnalyzerPro::metal

@implementation AnalyzerProMetalCoverView

- (BOOL)isFlipped
{
    return YES;
}

- (BOOL)acceptsFirstResponder
{
    return YES;
}

- (void)mouseDown:(NSEvent*)event        { if (owner != nullptr) owner->forwardMouseEvent (self, event); }
- (void)mouseUp:(NSEvent*)event          { if (owner != nullptr) owner->forwardMouseEvent (self, event); }
- (void)mouseDragged:(NSEvent*)event     { if (owner != nullptr) owner->forwardMouseEvent (self, event); }
- (void)mouseMoved:(NSEvent*)event       { if (owner != nullptr) owner->forwardMouseEvent (self, event); }
- (void)rightMouseDown:(NSEvent*)event   { if (owner != nullptr) owner->forwardMouseEvent (self, event); }
- (void)rightMouseUp:(NSEvent*)event     { if (owner != nullptr) owner->forwardMouseEvent (self, event); }
- (void)rightMouseDragged:(NSEvent*)event { if (owner != nullptr) owner->forwardMouseEvent (self, event); }
- (void)otherMouseDown:(NSEvent*)event   { if (owner != nullptr) owner->forwardMouseEvent (self, event); }
- (void)otherMouseUp:(NSEvent*)event     { if (owner != nullptr) owner->forwardMouseEvent (self, event); }
- (void)otherMouseDragged:(NSEvent*)event { if (owner != nullptr) owner->forwardMouseEvent (self, event); }
- (void)scrollWheel:(NSEvent*)event      { if (owner != nullptr) owner->forwardWheelEvent (self, event); }
- (void)keyDown:(NSEvent*)event          { if (owner != nullptr) owner->forwardKeyDown (event); }
- (void)keyUp:(NSEvent*)event            { if (owner != nullptr) owner->forwardKeyUp (event); }

@end

namespace AnalyzerPro::metal
{

MetalHost::MetalHost() = default;

MetalHost::~MetalHost()
{
    stop();
}

bool MetalHost::start (juce::Component& editor, MetalHostMechanism mechanism, const AnalyzerEngine* analyzerEngine)
{
    stop();
    impl_ = std::make_unique<MetalHostImpl> (editor, mechanism, analyzerEngine);
    return impl_->start();
}

void MetalHost::stop()
{
    if (impl_ == nullptr)
        return;

    if (! impl_->stop())
        NSLog (@"[MetalHost] teardown did not complete before reset");

    impl_.reset();
}

void MetalHost::resized()
{
    if (impl_ != nullptr)
        impl_->resized();
}

void MetalHost::setChromeFrame (std::shared_ptr<const FrameTexturePayload> frame)
{
    if (impl_ != nullptr)
        impl_->setChromeFrame (std::move (frame));
}

void MetalHost::setAnalyzerFrame (std::shared_ptr<const MetalAnalyzerFrame> frame)
{
    if (impl_ != nullptr)
        impl_->setAnalyzerFrame (std::move (frame));
}

float MetalHost::getBackingScaleFactor() const noexcept
{
    return impl_ != nullptr ? impl_->getBackingScaleFactor() : 1.0f;
}

bool MetalHost::isRunning() const noexcept
{
    return impl_ != nullptr && impl_->isRunning();
}

MetalHostMechanism MetalHost::getMechanism() const noexcept
{
    return impl_ != nullptr ? impl_->getMechanism() : MetalHostMechanism::BackingLayer;
}

} // namespace AnalyzerPro::metal

#endif // JUCE_MAC
