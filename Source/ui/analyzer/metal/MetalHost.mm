#include "MetalHost.h"
#include "../../../analyzer/AnalyzerEngine.h"

#if JUCE_MAC

#import <AppKit/AppKit.h>
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>
#import <QuartzCore/QuartzCore.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstring>
#include <cstddef>
#include <limits>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

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

@interface AnalyzerProMetalDisplayLinkTarget : NSObject
{
@public
    AnalyzerPro::metal::MetalHostImpl* owner;
}
- (void)step:(id)sender;
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
constexpr size_t kChromeTextureRingSize = 3;
constexpr int kNoChromeTextureIndex = -1;
constexpr auto kDisplayLinkThreadStartTimeout = std::chrono::milliseconds (1000);

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
        stop();
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

        if (renderPassDescriptor == nil)
            renderPassDescriptor = [[MTLRenderPassDescriptor renderPassDescriptor] retain];
        if (renderPassDescriptor == nil)
            return false;

        initialiseRenderPipelines();
        initialised = true;

        // Attach + start the display link now IF the editor already has a native peer. At
        // editor-construction time it usually does NOT — JUCE assigns the peer AFTER the
        // constructor returns — so this attach legitimately defers and completes later in
        // componentPeerChanged(). Metal being *available* (device/queue/layer created) is what
        // counts as success here, NOT being attached yet. We return false only on a genuine
        // Metal-resource failure (preserving the CPU fallback). This restores the Phase-0
        // behaviour that 1A regressed by treating "peer not ready" as fatal and tearing the
        // host down before the peer-attach retry could fire.
        (void) tryAttachAndStartLink();
        return true;
    }

    // Attaches the Metal layer to the editor's native peer view and starts the display link.
    // Safe to call repeatedly: a no-op once running, and a no-op (returns false) while the peer
    // is not yet available. Called from start() and re-tried from componentPeerChanged().
    bool tryAttachAndStartLink()
    {
        if (running)
            return true;
        if (! initialised)
            return false;

        if (! attachIfPossible())
            return false; // peer not ready yet — componentPeerChanged() will retry

        stopping.store (false, std::memory_order_release);
        if (! startDisplayLinkOnRenderThread())
            return false;

        running = true;
        NSLog (@"[MetalHost #%d] driver=CADisplayLink render_thread=dedicated", instanceId);
        return true;
    }

    void stop()
    {
        stopping.store (true, std::memory_order_release);
        stopDisplayLinkAndDrain ("stop");

        detachFromPeerView();

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
        lastRenderFrameTime = 0.0;
        gMetalHostFps.store (0.0f, std::memory_order_relaxed);
        gMetalHostEncodeMs.store (0.0f, std::memory_order_relaxed);
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
            stopDisplayLinkAndDrain ("peer lost");
            detachFromPeerView();
            gMetalHostFps.store (0.0f, std::memory_order_relaxed);
            return;
        }

        if (! running)
        {
            detachFromPeerView();
            (void) tryAttachAndStartLink();
            return;
        }

        stopDisplayLinkAndDrain ("peer changed");
        detachFromPeerView();
        (void) tryAttachAndStartLink();
    }

    void componentVisibilityChanged() override
    {
        if (editor.getPeer() == nullptr)
        {
            stopDisplayLinkAndDrain ("visibility peer lost");
            gMetalHostFps.store (0.0f, std::memory_order_relaxed);
            return;
        }

        if (! editor.isShowing())
        {
            stopDisplayLinkAndDrain ("hidden");
            gMetalHostFps.store (0.0f, std::memory_order_relaxed);
            return;
        }

        if (! running)
            (void) tryAttachAndStartLink();
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
                [event isDirectionInvertedFromDevice],
                [event hasPreciseScrollingDeltas],
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

    void displayLinkStep (id)
    {
        if (renderThreadShouldExit.load (std::memory_order_acquire)
            || stopping.load (std::memory_order_acquire))
        {
            return;
        }

        renderFrame();
    }

private:
    bool startDisplayLinkOnRenderThread()
    {
        if (peerView == nil)
            return false;

        if (displayLinkTarget == nil)
            displayLinkTarget = [[AnalyzerProMetalDisplayLinkTarget alloc] init];
        if (displayLinkTarget == nil)
            return false;
        displayLinkTarget->owner = this;

        id newDisplayLink = [peerView displayLinkWithTarget: displayLinkTarget
                                                    selector: @selector (step:)];
        if (newDisplayLink == nil)
        {
            NSScreen* screen = [[peerView window] screen];
            if (screen == nil)
                screen = [NSScreen mainScreen];
            newDisplayLink = [screen displayLinkWithTarget: displayLinkTarget
                                                   selector: @selector (step:)];
        }
        if (newDisplayLink == nil)
            return false;

        displayLink = [newDisplayLink retain];
        [displayLink setPaused: NO];

        {
            std::lock_guard<std::mutex> lock (displayLinkMutex);
            renderRunLoop = nullptr;
            renderThreadReady = false;
            renderThreadExited = false;
        }
        renderThreadShouldExit.store (false, std::memory_order_release);

        renderThread = std::thread ([this]
        {
            renderThreadMain();
        });

        std::unique_lock<std::mutex> lock (displayLinkMutex);
        const bool ready = displayLinkCv.wait_for (lock,
                                                   kDisplayLinkThreadStartTimeout,
                                                   [this]
                                                   {
                                                       return renderThreadReady || renderThreadExited;
                                                   });
        if (! ready || ! renderThreadReady)
        {
            lock.unlock();
            stopDisplayLinkAndDrain ("thread start failed");
            return false;
        }

        return true;
    }

    void renderThreadMain()
    {
        @autoreleasepool
        {
            CFRunLoopRef currentRunLoop = CFRunLoopGetCurrent();
            CFRetain (currentRunLoop);

            {
                std::lock_guard<std::mutex> lock (displayLinkMutex);
                renderRunLoop = currentRunLoop;
            }

            {
                id link = displayLink;
                if (link != nil)
                    [link addToRunLoop: [NSRunLoop currentRunLoop] forMode: NSRunLoopCommonModes];
            }

            {
                std::lock_guard<std::mutex> lock (displayLinkMutex);
                renderThreadReady = true;
            }
            displayLinkCv.notify_all();

            CFRunLoopRun();

            {
                std::lock_guard<std::mutex> lock (displayLinkMutex);
                if (renderRunLoop != nullptr)
                {
                    CFRelease (renderRunLoop);
                    renderRunLoop = nullptr;
                }
                renderThreadExited = true;
            }
            displayLinkCv.notify_all();
        }
    }

    void stopDisplayLinkAndDrain (const char* reason)
    {
        stopping.store (true, std::memory_order_release);
        renderThreadShouldExit.store (true, std::memory_order_release);
        running = false;

        id link = displayLink;
        if (link != nil)
            [link invalidate];

        CFRunLoopRef runLoopToStop = nullptr;
        {
            std::lock_guard<std::mutex> lock (displayLinkMutex);
            runLoopToStop = renderRunLoop;
            if (runLoopToStop != nullptr)
                CFRetain (runLoopToStop);
        }

        if (runLoopToStop != nullptr)
        {
            CFRunLoopStop (runLoopToStop);
            CFRelease (runLoopToStop);
        }

        if (renderThread.joinable())
        {
            if (renderThread.get_id() == std::this_thread::get_id())
                renderThread.detach();
            else
                renderThread.join();
        }

        if (displayLink != nil)
        {
            [displayLink release];
            displayLink = nil;
        }

        if (displayLinkTarget != nil)
        {
            displayLinkTarget->owner = nullptr;
            [displayLinkTarget release];
            displayLinkTarget = nil;
        }

        {
            std::lock_guard<std::mutex> lock (displayLinkMutex);
            renderThreadReady = false;
            renderThreadExited = true;
            renderRunLoop = nullptr;
        }

        (void) reason;
        lastRenderFrameTime = 0.0;
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

    void detachFromPeerView()
    {
        if (coverView != nil)
        {
            coverView->owner = nullptr;
            [coverView removeFromSuperview];
            [coverView release];
            coverView = nil;
        }

        if (peerView != nil && mechanism == MetalHostMechanism::BackingLayer)
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

    void updateLayerGeometry()
    {
        if (metalLayer == nil)
            return;

        NSView* hostView = (coverView != nil) ? coverView : peerView;
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

        analyzerFillBuffer = [device newBufferWithLength: sizeof (ColourVertex) * kMaxAnalyzerFillVertices
                                                 options: MTLResourceStorageModeShared];
        analyzerLineBuffer = [device newBufferWithLength: sizeof (ColourVertex) * kMaxAnalyzerBins
                                                 options: MTLResourceStorageModeShared];
        analyzerFillVertices.resize (kMaxAnalyzerFillVertices);
        analyzerLineVertices.resize (kMaxAnalyzerBins);
        analyzerSmoothedDb.fill (-200.0f);

        renderPipelinesReady = chromeSampler != nil
            && chromeQuadBuffer != nil
            && analyzerFillBuffer != nil
            && analyzerLineBuffer != nil;
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

    void renderFrame()
    {
        if (stopping.load (std::memory_order_acquire) || metalLayer == nil || commandQueue == nil)
            return;

        inFlightFrames.fetch_add (1, std::memory_order_acq_rel);
        if (stopping.load (std::memory_order_acquire))
        {
            inFlightFrames.fetch_sub (1, std::memory_order_acq_rel);
            return;
        }

        @autoreleasepool
        {
            id<CAMetalDrawable> drawable = [metalLayer nextDrawable];
            if (drawable != nil)
            {
                [drawable retain];
                id<MTLCommandBuffer> commandBuffer = [commandQueue commandBuffer];
                if (commandBuffer == nil)
                {
                    [drawable release];
                    inFlightFrames.fetch_sub (1, std::memory_order_acq_rel);
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
                gMetalHostEncodeMs.store (static_cast<float> ((CACurrentMediaTime() - encodeStart) * 1000.0),
                                          std::memory_order_relaxed);

                [CATransaction begin];
                [CATransaction setDisableActions: YES];
                [commandBuffer presentDrawable: drawable];
                id<CAMetalDrawable> retainedDrawable = drawable;
                [commandBuffer addCompletedHandler: ^(id<MTLCommandBuffer>)
                {
                    [retainedDrawable release];
                }];
                [commandBuffer commit];
                [CATransaction commit];

                updateFps();
            }
        }

        inFlightFrames.fetch_sub (1, std::memory_order_acq_rel);
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

    static float timeConstantAlpha (float timeMs, double dtSeconds) noexcept
    {
        if (timeMs <= 0.0f || dtSeconds <= 0.0)
            return 1.0f;

        const double tauSeconds = static_cast<double> (timeMs) / 1000.0;
        return static_cast<float> (juce::jlimit (0.0, 1.0, 1.0 - std::exp (-dtSeconds / tauSeconds)));
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
        if (analyzerEngine == nullptr || ! frame.valid)
            return 0;

        if (! analyzerEngine->getLatestSnapshot (renderSnapshot))
            return analyzerPipelinePrimed ? analyzerPipelineBins : 0;

        const auto holdLastGood = [this]() noexcept -> size_t
        {
            return analyzerPipelinePrimed ? analyzerPipelineBins : 0;
        };

        const int snapshotBins = (renderSnapshot.fftBinCount > 0) ? renderSnapshot.fftBinCount : renderSnapshot.numBins;
        const int expectedBins = renderSnapshot.fftSize / 2 + 1;
        if (! renderSnapshot.isValid
            || snapshotBins <= 1
            || expectedBins <= 1
            || snapshotBins != expectedBins
            || renderSnapshot.sampleRate <= 0.0
            || renderSnapshot.fftSize <= 0)
        {
            return holdLastGood();
        }

        const size_t validBins = std::min (static_cast<size_t> (snapshotBins), kMaxAnalyzerBins);
        if (validBins <= 1)
            return holdLastGood();

        const bool resetState = ! analyzerPipelinePrimed
            || analyzerPipelineBins != validBins
            || analyzerPipelineFftSize != renderSnapshot.fftSize
            || std::abs (analyzerPipelineSampleRate - renderSnapshot.sampleRate) > 0.001;

        analyzerPipelineBins = validBins;
        analyzerPipelineFftSize = renderSnapshot.fftSize;
        analyzerPipelineSampleRate = renderSnapshot.sampleRate;

        const float attackAlpha = timeConstantAlpha (frame.rmsAttackMs, renderDtSeconds);
        const float releaseAlpha = timeConstantAlpha (frame.rmsReleaseMs, renderDtSeconds);

        for (size_t i = 0; i < validBins; ++i)
        {
            // The snapshot already contains the shared engine's dB conversion, weighting, and
            // spectral smoothing. This render-owned layer advances the display ballistics at
            // display-link cadence so repeated snapshots still move smoothly between updates.
            const float targetDb = sanitizeAnalyzerDb (renderSnapshot.fftDb[i]);
            if (resetState)
            {
                analyzerSmoothedDb[i] = targetDb;
                continue;
            }

            const float currentDb = analyzerSmoothedDb[i];
            const float alpha = (targetDb > currentDb) ? attackAlpha : releaseAlpha;
            analyzerSmoothedDb[i] = sanitizeAnalyzerDb (currentDb + (targetDb - currentDb) * alpha);
        }

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
            const float x = frame.plotRectPx.x + xNorm * frame.plotRectPx.w;
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

    void drawAnalyzerFrame (id<MTLRenderCommandEncoder> encoder,
                            id<MTLTexture> drawableTexture,
                            const std::shared_ptr<const MetalAnalyzerFrame>& frame,
                            double renderDtSeconds)
    {
        if (frame == nullptr)
            return;

        const size_t validBins = updateAnalyzerPipelineFromSnapshot (*frame, renderDtSeconds);
        if (validBins <= 1)
            return;

        size_t fillVertexCount = 0;
        size_t lineVertexCount = 0;
        if (! buildAnalyzerVertices (*frame,
                                     validBins,
                                     static_cast<float> ([drawableTexture width]),
                                     static_cast<float> ([drawableTexture height]),
                                     fillVertexCount,
                                     lineVertexCount))
        {
            return;
        }

        std::memcpy ([analyzerFillBuffer contents],
                     analyzerFillVertices.data(),
                     sizeof (ColourVertex) * fillVertexCount);
        std::memcpy ([analyzerLineBuffer contents],
                     analyzerLineVertices.data(),
                     sizeof (ColourVertex) * lineVertexCount);

        const NSUInteger drawableWidth = [drawableTexture width];
        const NSUInteger drawableHeight = [drawableTexture height];
        const NSUInteger scissorX = static_cast<NSUInteger> (juce::jlimit (0.0f, static_cast<float> (drawableWidth), frame->plotRectPx.x));
        const NSUInteger scissorY = static_cast<NSUInteger> (juce::jlimit (0.0f, static_cast<float> (drawableHeight), frame->plotRectPx.y));
        const NSUInteger scissorRight = static_cast<NSUInteger> (juce::jlimit (0.0f, static_cast<float> (drawableWidth), frame->plotRectPx.x + frame->plotRectPx.w));
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
        [encoder setVertexBuffer: analyzerFillBuffer offset: 0 atIndex: 0];
        [encoder drawPrimitives: MTLPrimitiveTypeTriangleStrip
                     vertexStart: 0
                     vertexCount: static_cast<NSUInteger> (fillVertexCount)];

        [encoder setVertexBuffer: analyzerLineBuffer offset: 0 atIndex: 0];
        [encoder drawPrimitives: MTLPrimitiveTypeLineStrip
                     vertexStart: 0
                     vertexCount: static_cast<NSUInteger> (lineVertexCount)];

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
        [descriptor release];

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
            return;
        }

        ++fpsFrames;
        const double elapsed = now - lastFpsTime;

        if (elapsed >= 1.0)
        {
            const float computedFps = static_cast<float> (static_cast<double> (fpsFrames) / elapsed);
            gMetalHostFps.store (computedFps, std::memory_order_relaxed);
            NSLog (@"[MetalHost #%d] driver=CADisplayLink thread=%s mech=%s fps=%.1f encode_ms=%.3f",
                   instanceId,
                   [NSThread isMainThread] ? "main" : "off-main",
                   getMetalHostMechanismName (mechanism),
                   static_cast<double> (computedFps),
                   static_cast<double> (gMetalHostEncodeMs.load (std::memory_order_relaxed)));
            fpsFrames = 0;
            lastFpsTime = now;
        }
    }

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
    std::array<id<MTLTexture>, kChromeTextureRingSize> chromeTextures {};
    int chromeTextureWidth = 0;
    int chromeTextureHeight = 0;
    uint64_t uploadedChromeSequence = 0;
    size_t nextChromeTextureUploadSlot = 0;
    std::atomic<int> currentChromeTextureIndex { kNoChromeTextureIndex };
    id displayLink = nil;
    AnalyzerProMetalDisplayLinkTarget* displayLinkTarget = nil;
    std::thread renderThread;
    std::mutex displayLinkMutex;
    std::condition_variable displayLinkCv;
    CFRunLoopRef renderRunLoop = nullptr;
    bool renderThreadReady = false;
    bool renderThreadExited = true;
    std::atomic<bool> renderThreadShouldExit { true };
    NSView* peerView = nil;
    CALayer* originalLayer = nil;
    AnalyzerProMetalCoverView* coverView = nil;
    BOOL originalWantsLayer = NO;
    bool layerAttached = false;
    bool initialised = false;
    bool running = false;
    std::atomic<bool> stopping { false };
    std::atomic<int> inFlightFrames { 0 };
    std::atomic<float> backingScale { 1.0f };
    std::shared_ptr<const FrameTexturePayload> latestChromeFrame;
    std::shared_ptr<const MetalAnalyzerFrame> latestAnalyzerFrame;
    AnalyzerSnapshot renderSnapshot;
    std::array<float, kMaxAnalyzerBins> analyzerSmoothedDb {};
    std::vector<ColourVertex> analyzerFillVertices;
    std::vector<ColourVertex> analyzerLineVertices;
    double analyzerPipelineSampleRate = 0.0;
    int analyzerPipelineFftSize = 0;
    size_t analyzerPipelineBins = 0;
    bool analyzerPipelinePrimed = false;
    double lastFpsTime = 0.0;
    double lastRenderFrameTime = 0.0;
    int fpsFrames = 0;
    int instanceId = 0;
    bool renderPipelinesReady = false;

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

@implementation AnalyzerProMetalDisplayLinkTarget

- (void)step:(id)sender
{
    if (owner != nullptr)
        owner->displayLinkStep (sender);
}

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
