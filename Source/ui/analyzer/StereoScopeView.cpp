#include "StereoScopeView.h"
#include "../../config/UiRates.h"
#include <cmath>

namespace
{
    constexpr int kRenderPadX = 2;   // symmetric padding so rendered scope stays square
}

StereoScopeView::StereoScopeView (mdsp_ui::UiContext& ui, StereoScopeAnalyzer& analyzer)
    : ui_ (ui), analyzer_ (analyzer)
{
    // Initialize buffers to reasonable snapshot size (e.g. 512 samples)
    lBuffer_.resize (512, 0.0f);
    rBuffer_.resize (512, 0.0f);
    
    startTimerHz (AnalyzerPro::UiRates::kScopeHz);
}

StereoScopeView::~StereoScopeView()
{
    stopTimer();
}

void StereoScopeView::resized()
{
    auto area = getLocalBounds();
    if (area.isEmpty())
        return;

    // Enforce square plot inside the view
    const int side = juce::jmin (area.getWidth(), area.getHeight());

    auto plot = juce::Rectangle<int> (side, side)
        .withCentre (area.getCentre())
        .reduced (kRenderPadX); // symmetric padding only

    if (plot.getWidth() <= 0 || plot.getHeight() <= 0)
        return;

    // Allocate square backing images
    accumImage_ = juce::Image (juce::Image::ARGB,
                               plot.getWidth(),
                               plot.getHeight(),
                               true);

    heldImage_  = juce::Image (juce::Image::ARGB,
                               plot.getWidth(),
                               plot.getHeight(),
                               true);
}

void StereoScopeView::timerCallback()
{
    // Fetch latest snapshot
    int numSamples = analyzer_.getSnapshot (lBuffer_, rBuffer_, static_cast<int> (lBuffer_.size()));
    
    if (numSamples > 0 && !accumImage_.isNull()
        && accumImage_.getWidth() > 0 && accumImage_.getHeight() > 0)
    {
        // Decay existing image (only if hold is OFF)
        if (!holdEnabled_)
        {
            accumImage_.multiplyAllAlphas (decayFactor_);
        }
        
        renderScopeToImage (numSamples);
        
        // If hold enabled, composite with max logic
        if (holdEnabled_ && !heldImage_.isNull()
            && heldImage_.getWidth() > 0 && heldImage_.getHeight() > 0)
        {
            // Composite: for each pixel, take max alpha (brightest)
            // Simple approach: just overlay new on held
            juce::Graphics hg (heldImage_);
            hg.drawImageAt (accumImage_, 0, 0);
        }
        
        repaint();
    }
}

void StereoScopeView::renderScopeToImage(int numSamples)
{
    if (accumImage_.isNull()) return;
    if (accumImage_.getWidth() <= 0 || accumImage_.getHeight() <= 0) return;

    // Step 6: Safety check for buffer sizes
    // Ensure we don't read beyond buffer bounds
    const int maxL = static_cast<int> (lBuffer_.size());
    const int maxR = static_cast<int> (rBuffer_.size());
    const int limit = juce::jmin (numSamples, maxL, maxR);
    
    if (limit <= 0) return;

    juce::Graphics g (accumImage_);
    juce::Image::BitmapData scatterBitmap (accumImage_, juce::Image::BitmapData::readWrite);
    
    // M/S Mapping:
    // X = Side = (L - R)
    // Y = Mid  = (L + R)
    // Coordinate system: Center (0,0) is screen center.
    // X increases Right. Y increases Up (so invert for screen Y).
    // Rotation logic: L is -45deg (Left Top), R is +45deg (Right Top).
    
    const auto& theme = ui_.theme();
    const float w = static_cast<float> (accumImage_.getWidth());
    const float h = static_cast<float> (accumImage_.getHeight());
    const float cx = w * 0.5f;
    const float cy = h * 0.5f;
    const float radius = juce::jmin (cx, cy) * scale_;
    
    // RMS Smoothing Logic
    // If RMS Mode, we smooth the input L/R signals before mapping
    if (scopeMode_ == ScopeMode::RMS)
    {
         if (lSmoothed_.size() != lBuffer_.size()) { lSmoothed_.resize(lBuffer_.size(), 0.0f); std::fill(lSmoothed_.begin(), lSmoothed_.end(), 0.0f); }
         if (rSmoothed_.size() != rBuffer_.size()) { rSmoothed_.resize(rBuffer_.size(), 0.0f); std::fill(rSmoothed_.begin(), rSmoothed_.end(), 0.0f); }
         
         // Simple temporal smoothing? No, that's frame-to-frame.
         // "RMS Scope" usually means slow ballistics.
         // But here we are plotting a snapshot buffer of N samples.
         // If we smooth *within* the buffer, we lose high frequency detail (Lissajous becomes rounder).
         // If we smooth *frame-to-frame*, the whole blob lags. 
         // Let's assume user wants "slower, less jittery" blob.
         // Simple approach: Moving Average on the input buffer?
         // Or just apply simple Low Pass on the coordinates?
         // Let's apply simple 2-point moving average to reduce jitter for now, or just trust raw samples if "RMS" just means "Display as Cloud".
         // Actually, most "RMS" scopes just integrate over small window.
         // Let's stick to raw samples for now but draw them as cloud (scatter) if Shape is PAZ.
         // Wait, ScopeMode and Shape are orthogonal.
         // If Mode=RMS, maybe we should square/sqrt? No, scope is phase correlation.
         // Let's interpret "RMS Mode" as "Slower Decay / Integration".
         // But decay is global.
         // Let's implement ScopeMode::RMS as "Highlight High Energy" or simple LPF.
         // Use a simple LPF on the buffer in place for display?
         
         // Implementation: Simple LPF
         float lState = 0.0f;
         float rState = 0.0f;
         const float coeff = 0.15f; 
         
         const size_t count = lBuffer_.size();
         for(size_t i=0; i<count; ++i)
         {
             lState += coeff * (lBuffer_[i] - lState);
             rState += coeff * (rBuffer_[i] - rState);
             lSmoothed_[i] = lState;
             rSmoothed_[i] = rState;
         }
    }

    g.setColour (theme.seriesPeak.withAlpha (0.9f)); // High contrast trace
    
    juce::Path p;
    bool first = true;
    
    // Processing loop
    // Step 5: Channel-count guard (limit loop to valid samples)
    const size_t count = static_cast<size_t> (limit);
    
    // Optimization: scatter plot can just set pixels or draw small rects
    // Graphics::drawPoint doesn't exist? fillRect(x,y,1,1)
    
    for (size_t i = 0; i < count; ++i)
    {
        // Safety: ensure indices are valid (redundant with limit but safe)
        if (i >= lBuffer_.size() || i >= rBuffer_.size()) break;

        float l = (scopeMode_ == ScopeMode::RMS && i < lSmoothed_.size()) ? lSmoothed_[i] : lBuffer_[i];
        float r = (scopeMode_ == ScopeMode::RMS && i < rSmoothed_.size()) ? rSmoothed_[i] : rBuffer_[i];
        
        // Coordinate Calculation based on Channel Mode
        float sx = 0.0f;
        float sy = 0.0f;

        if (channelMode_ == ChannelMode::MidSide)
        {
            // Classic Vectorscope (Mid/Side)
            // Side = L - R (X axis)
            // Mid  = L + R (Y axis)
            float side = (l - r) * 0.5f; 
            float mid  = (l + r) * 0.5f; 
            
            sx = cx + side * radius * 2.5f; 
            sy = cy - mid * radius * 2.5f;
        }
        else
        {
            // Stereo Scope (X-Y Plot)
            // X = L
            // Y = R
            // This plots Left on X axis, Right on Y axis.
            // Range [-1, 1] mapped to radius.
            
            sx = cx + l * radius; 
            sy = cy - r * radius;
        }
        
        // Guard against NaN/Inf from bad input (avoids CoreGraphics crash in fillRect)
        if (!std::isfinite (sx) || !std::isfinite (sy))
            continue;

        if (scopeShape_ == ScopeShape::Lissajous)
        {
            if (first)
            {
                p.startNewSubPath (sx, sy);
                first = false;
            }
            else
            {
                p.lineTo (sx, sy);
            }
        }
        else // Scatter: write pixels directly to the backing image (avoid CGContext fillRect crash path)
        {
            const int px = juce::jlimit (0, static_cast<int> (w) - 1, static_cast<int> (std::floor (sx)));
            const int py = juce::jlimit (0, static_cast<int> (h) - 1, static_cast<int> (std::floor (sy)));

            const auto putPixel = [&scatterBitmap](int x, int y)
            {
                if (x < 0 || y < 0 || x >= scatterBitmap.width || y >= scatterBitmap.height)
                    return;

                auto* pxPtr = scatterBitmap.getPixelPointer (x, y);
                const juce::uint8 alpha = pxPtr[3];
                pxPtr[0] = 0x3a; // B
                pxPtr[1] = 0xd8; // G
                pxPtr[2] = 0xff; // R
                pxPtr[3] = static_cast<juce::uint8> (juce::jmin (255, static_cast<int> (alpha) + 96));
            };

            putPixel (px, py);
            putPixel (px + 1, py);
            putPixel (px, py + 1);
            putPixel (px + 1, py + 1);
        }
    }
    
    if (scopeShape_ == ScopeShape::Lissajous)
    {
        g.strokePath (p, juce::PathStrokeType (1.2f));
    }
}

void StereoScopeView::paint (juce::Graphics& g)
{
    const auto& theme = ui_.theme();
    g.fillAll (theme.panel);

    auto area = getLocalBounds().toFloat();

    const float side = juce::jmin (area.getWidth(), area.getHeight());

    auto plot = juce::Rectangle<float> (side, side)
        .withCentre (area.getCentre())
        .reduced (float (kRenderPadX));

    if (plot.getWidth() <= 0 || plot.getHeight() <= 0)
        return;

    float cx = plot.getCentreX();
    float cy = plot.getCentreY();

    g.setColour (theme.grid);
    g.drawVerticalLine (static_cast<int> (cx), plot.getY(), plot.getBottom());
    g.drawHorizontalLine (static_cast<int> (cy), plot.getX(), plot.getRight());

    g.setColour (theme.textMuted);
    g.setFont (ui_.type().labelSmallFont());

    if (channelMode_ == ChannelMode::MidSide)
    {
        g.drawText ("M", juce::Rectangle<float> (cx + 2, plot.getY() + 2, 20, 12), juce::Justification::topLeft, false);
        g.drawText ("S", juce::Rectangle<float> (plot.getRight() - 20, cy - 12, 15, 12), juce::Justification::centredRight, false);
    }
    else
    {
        g.drawText ("R", juce::Rectangle<float> (cx + 2, plot.getY() + 2, 20, 12), juce::Justification::topLeft, false);
        g.drawText ("L", juce::Rectangle<float> (plot.getRight() - 20, cy - 12, 15, 12), juce::Justification::centredRight, false);
    }

    auto plotInt = plot.toNearestInt();
    g.saveState();
    g.reduceClipRegion (plotInt);

    if (holdEnabled_ && !heldImage_.isNull())
        g.drawImageAt (heldImage_, plotInt.getX(), plotInt.getY());
    else if (!accumImage_.isNull())
        g.drawImageAt (accumImage_, plotInt.getX(), plotInt.getY());

    g.restoreState();

    g.setColour (theme.borderDivider);
    g.drawRect (area, 1.0f);
}

void StereoScopeView::setHoldEnabled (bool hold)
{
    holdEnabled_ = hold;
    
    if (!holdEnabled_)
    {
        // Reset held image when hold is turned off
        resetHold();
    }
}

void StereoScopeView::resetHold()
{
    if (!heldImage_.isNull())
    {
        heldImage_.clear (heldImage_.getBounds());
    }
}
