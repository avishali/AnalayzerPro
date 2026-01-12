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
}

void StereoScopeView::timerCallback()
{
    // Fetch latest snapshot
    int numSamples = analyzer_.getSnapshot (lBuffer_, rBuffer_, static_cast<int> (lBuffer_.size()));
    
    if (numSamples > 0)
    {
        // Decay existing image
        accumImage_.multiplyAllAlphas (decayFactor_);
        
        renderScopeToImage();
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
    
    // Draw points
    // We can draw a path or individual points?
    // Vectorscope usually draws a connected line (lissajous).
    // Or cloud of points. Let's try connected line for "trace".
    
    g.setColour (theme.seriesPeak.withAlpha (0.9f)); // High contrast trace
    
    juce::Path p;
    bool first = true;
    
    // Processing loop
    const size_t count = lBuffer_.size();
    for (size_t i = 0; i < count; ++i)
    {
        float l = lBuffer_[i];
        float r = rBuffer_[i];
        
        // Mid/Side Calculate
        // M = (L+R)/sqrt(2) usually, but simple L+R scales faster.
        // We want normalized [-1, 1] input to map to radius.
        // Input audio is usually [-1, 1].
        // Side = L - R (Range -2 to 2)
        // Mid  = L + R (Range -2 to 2)
        // Adjust scale accordingly.
        
        float side = (l - r) * 0.5f; // [-1, 1]
        float mid  = (l + r) * 0.5f; // [-1, 1]
        
        // Map to screen
        // X = cx + side * radius
        // Y = cy - mid * radius  (Inverted Y)
        
        float sx = cx + side * radius * 2.5f; // Explicit scale boost for visibility
        float sy = cy - mid * radius * 2.5f;
        
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
    
    // Using a path stroke might be heavy for 512 points?
    // 512 points is fine.
    g.strokePath (p, juce::PathStrokeType (1.2f));
}

void StereoScopeView::paint (juce::Graphics& g)
{
    const auto& theme = ui_.theme();
    const auto& metrics = ui_.metrics();
    
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
    
    // Use float rectangle for drawText
    g.drawText ("M", juce::Rectangle<float> (cx + 2, 2, 20, 12), juce::Justification::topLeft, false);
    g.drawText ("S", juce::Rectangle<float> (area.getRight() - 20, cy - 12, 15, 12), juce::Justification::centredRight, false);
    
    // Draw Accumulation Buffer
    if (!accumImage_.isNull())
    {
        g.drawImageAt (accumImage_, 0, 0);
    }
    
    // Border
    g.setColour (theme.borderDivider);
    g.drawRect (area, 1.0f);
}
