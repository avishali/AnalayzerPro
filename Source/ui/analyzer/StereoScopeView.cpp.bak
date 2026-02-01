#include "StereoScopeView.h"

StereoScopeView::StereoScopeView (mdsp_ui::UiContext& ui, StereoScopeAnalyzer& analyzer)
    : ui_ (ui), analyzer_ (analyzer)
{
    // Initialize buffers to reasonable snapshot size (e.g. 512 samples)
    lBuffer_.resize (512, 0.0f);
    rBuffer_.resize (512, 0.0f);
    
    startTimerHz (30); // 30 FPS update
}

StereoScopeView::~StereoScopeView()
{
    stopTimer();
}

void StereoScopeView::resized()
{
    // Re-allocate images on resize
    auto area = getLocalBounds();
    if (area.isEmpty()) return;
    
    accumImage_ = juce::Image (juce::Image::ARGB, area.getWidth(), area.getHeight(), true);
    heldImage_ = juce::Image (juce::Image::ARGB, area.getWidth(), area.getHeight(), true);
}

void StereoScopeView::timerCallback()
{
    // Fetch latest snapshot
    int numSamples = analyzer_.getSnapshot (lBuffer_, rBuffer_, static_cast<int> (lBuffer_.size()));
    
    if (numSamples > 0)
    {
        // Decay existing image (only if hold is OFF)
        if (!holdEnabled_)
        {
            accumImage_.multiplyAllAlphas (decayFactor_);
        }
        
        renderScopeToImage();
        
        // If hold enabled, composite with max logic
        if (holdEnabled_ && !heldImage_.isNull())
        {
            // Composite: for each pixel, take max alpha (brightest)
            // Simple approach: just overlay new on held
            juce::Graphics hg (heldImage_);
            hg.drawImageAt (accumImage_, 0, 0);
        }
        
        repaint();
    }
}

void StereoScopeView::renderScopeToImage()
{
    if (accumImage_.isNull()) return;

    juce::Graphics g (accumImage_);
    
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
    const size_t count = lBuffer_.size();
    
    // Optimization: scatter plot can just set pixels or draw small rects
    // Graphics::drawPoint doesn't exist? fillRect(x,y,1,1)
    
    for (size_t i = 0; i < count; ++i)
    {
        float l = (scopeMode_ == ScopeMode::RMS) ? lSmoothed_[i] : lBuffer_[i];
        float r = (scopeMode_ == ScopeMode::RMS) ? rSmoothed_[i] : rBuffer_[i];
        
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
        else // Scatter
        {
             g.fillRect (sx - 1.0f, sy - 1.0f, 2.0f, 2.0f);
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
    // Background
    g.fillAll (theme.panel);
    
    // Grid / Axes
    g.setColour (theme.grid);
    auto area = getLocalBounds().toFloat();
    float cx = area.getCentreX();
    float cy = area.getCentreY();
    
    // Diagonal lines (L/R axes)
    // L axis: y + x = c? No.
    // L only: R=0 -> M=L/2, S=L/2 -> X=L/2, Y=L/2. Diagonal /
    // R only: L=0 -> M=R/2, S=-R/2 -> X=-R/2, Y=R/2. Diagonal \
    
    // Center cross (Mid/Side axes)
    g.drawVerticalLine (static_cast<int> (cx), 0.0f, area.getHeight());
    g.drawHorizontalLine (static_cast<int> (cy), 0.0f, area.getWidth());
    
    // Diagonals (L/R)
    // Not easy to draw infinite diagonal in rect.
    // Just draw a cross 45 deg?
    // Let's stick to M/S grid for now (Vertical/Horizontal).
    
    // Draw Labels (L, R, M, S)
    g.setColour (theme.textMuted);
    g.setFont (ui_.type().labelSmallFont());
    
    // Draw Labels based on Mode
    if (channelMode_ == ChannelMode::MidSide)
    {
        g.drawText ("M", juce::Rectangle<float> (cx + 2, 2, 20, 12), juce::Justification::topLeft, false);
        g.drawText ("S", juce::Rectangle<float> (area.getRight() - 20, cy - 12, 15, 12), juce::Justification::centredRight, false);
    }
    else
    {
        // Stereo Mode Labels: Y-axis is R, X-axis is L
        g.drawText ("R", juce::Rectangle<float> (cx + 2, 2, 20, 12), juce::Justification::topLeft, false);
        g.drawText ("L", juce::Rectangle<float> (area.getRight() - 20, cy - 12, 15, 12), juce::Justification::centredRight, false);
    }
    
    // Draw Accumulation Buffer (or held if holding)
    if (holdEnabled_ && !heldImage_.isNull())
    {
        g.drawImageAt (heldImage_, 0, 0);
    }
    else if (!accumImage_.isNull())
    {
        g.drawImageAt (accumImage_, 0, 0);
    }
    
    // Border
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
