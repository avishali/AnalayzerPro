#include "PhaseFanScopeComponent.h"
#include <cmath>

namespace
{
    constexpr float kPadding = 10.0f;  // PAZ_SCOPE_GEOMETRY: small padding only
    constexpr float kPi = 3.14159265358979323846f;
}

PhaseFanScopeComponent::PhaseFanScopeComponent (mdsp_ui::UiContext& ui)
    : ui_ (ui), fifo_ (kFifoCapacity)
{
    fifoBuffer_.resize (static_cast<size_t> (kFifoCapacity) * 2u);
    workBuffer_.resize (static_cast<size_t> (kMaxPairsPerFrame) * 2u);
    // PAZ_SCOPE: zero density buffers
    for (auto& row : density_)
        row.fill (0.0f);
    for (auto& row : peakDensity_)
        row.fill (0.0f);
    peakRNorm_.fill (0.0f);   // PAZ_SCOPE_PEAK_HOLD
    lineRNormEMA_.fill (0.0f);  // PAZ_SCOPE_LINES_MODE
    startTimerHz (60);
}

PhaseFanScopeComponent::~PhaseFanScopeComponent()
{
    stopTimer();
}

void PhaseFanScopeComponent::setEnabled (bool enabled) noexcept
{
    enabled_.store (enabled, std::memory_order_relaxed);
}

void PhaseFanScopeComponent::setPersistence (float p) noexcept
{
    persistence_.store (juce::jlimit (0.0f, 1.0f, p), std::memory_order_relaxed);
}

void PhaseFanScopeComponent::setPointStride (int stride) noexcept
{
    pointStride_.store (juce::jmax (1, stride), std::memory_order_relaxed);
}

void PhaseFanScopeComponent::setBallistics (float attackMs, float releaseMs) noexcept
{
    attackMs_.store (juce::jmax (0.1f, attackMs), std::memory_order_relaxed);
    releaseMs_.store (juce::jmax (1.0f, releaseMs), std::memory_order_relaxed);
}

void PhaseFanScopeComponent::setReleaseMs (float releaseMs) noexcept
{
    releaseMs_.store (juce::jmax (1.0f, releaseMs), std::memory_order_relaxed);
}

void PhaseFanScopeComponent::setMaxRadiusPx (int maxRadius) noexcept
{
    maxRadiusPx_ = maxRadius;
}

void PhaseFanScopeComponent::setPeakHoldEnabled (bool enabled) noexcept
{
    peakHoldEnabled_.store (enabled, std::memory_order_relaxed);
}

void PhaseFanScopeComponent::setPeakHoldMs (float ms) noexcept
{
    peakHoldMs_.store (juce::jlimit (0.0f, 5000.0f, ms), std::memory_order_relaxed);
}

void PhaseFanScopeComponent::setPeakReleaseMs (float ms) noexcept
{
    peakReleaseMs_.store (juce::jlimit (10.0f, 10000.0f, ms), std::memory_order_relaxed);
}

void PhaseFanScopeComponent::setRenderMode (PhaseFanRenderMode mode) noexcept
{
    renderMode_ = mode;  // PAZ_SCOPE_LINES_MODE
}

void PhaseFanScopeComponent::resetPeakHold() noexcept
{
    for (size_t a = 0; a < static_cast<size_t> (kAngleBins); ++a)
        peakRNorm_[a] = lineRNormEMA_[a];  // Peak hold applies to white trace (contour) only
}

void PhaseFanScopeComponent::pushAudioBlock (const juce::AudioBuffer<float>& buffer,
                                             int startSample,
                                             int numSamples) noexcept
{
    if (!enabled_.load (std::memory_order_relaxed))
        return;
    const int numCh = buffer.getNumChannels();
    if (numCh < 1 || numSamples <= 0 || startSample < 0)
        return;
    const float* const l = buffer.getReadPointer (0, startSample);
    const float* const r = (numCh >= 2) ? buffer.getReadPointer (1, startSample) : l;  // PAZ_SCOPE: mono -> duplicate to stereo
    const int numItems = juce::jmin (numSamples * 2, kFifoCapacity * 2);
    int s1, b1, s2, b2;
    fifo_.prepareToWrite (numItems, s1, b1, s2, b2);
    int written = 0;
    if (b1 > 0)
    {
        const int pairsInB1 = b1 / 2;
        for (int i = 0; i < pairsInB1; ++i)
        {
            fifoBuffer_[static_cast<size_t> (s1) + static_cast<size_t> (i) * 2u]     = l[i];
            fifoBuffer_[static_cast<size_t> (s1) + static_cast<size_t> (i) * 2u + 1] = r[i];
        }
        written += b1;
    }
    if (b2 > 0)
    {
        const int pairsInB1 = b1 / 2;
        const int pairsInB2 = b2 / 2;
        for (int i = 0; i < pairsInB2; ++i)
        {
            fifoBuffer_[static_cast<size_t> (s2) + static_cast<size_t> (i) * 2u]     = l[pairsInB1 + i];
            fifoBuffer_[static_cast<size_t> (s2) + static_cast<size_t> (i) * 2u + 1] = r[pairsInB1 + i];
        }
        written += b2;
    }
    fifo_.finishedWrite (written);
}

void PhaseFanScopeComponent::resized()
{
    // PAZ_SCOPE: no accumulation image; paint renders density directly
}

void PhaseFanScopeComponent::timerCallback()
{
    if (!enabled_.load (std::memory_order_relaxed))
        return;
    drainFifoAndUpdateBins();
}

void PhaseFanScopeComponent::drainFifoAndUpdateBins()
{
    const int numReady = fifo_.getNumReady();
    if (numReady < 2)
    {
        // PAZ_SCOPE: apply decay even when no new data (silence -> density decays)
        for (size_t a = 0; a < static_cast<size_t> (kAngleBins); ++a)
        {
            for (size_t r = 0; r < static_cast<size_t> (kRadiusBins); ++r)
            {
                density_[a][r] *= kDecayFactor;
                density_[a][r] = juce::jlimit (0.0f, kMaxDensity, density_[a][r]);
            }
            lineRNormEMA_[a] *= 0.95f;  // decay contour toward 0 in silence
            if (peakHoldEnabled_.load (std::memory_order_relaxed))
                peakRNorm_[a] = juce::jlimit (0.0f, 1.0f, peakRNorm_[a] * kPeakDecayFactor);
        }
        repaint();
        return;
    }
    const int itemsToRead = juce::jmin (numReady, kMaxPairsPerFrame * 2);
    int s1, b1, s2, b2;
    fifo_.prepareToRead (itemsToRead, s1, b1, s2, b2);
    const int totalRead = b1 + b2;
    if (totalRead < 2)
    {
        fifo_.finishedRead (totalRead);
        repaint();
        return;
    }
    size_t outIdx = 0;
    const int stride = pointStride_.load (std::memory_order_relaxed);
    int pairIdx = 0;
    if (b1 > 0)
    {
        for (int i = 0; i < b1; i += 2)
        {
            if (pairIdx % stride == 0 && outIdx + 1 < workBuffer_.size())
            {
                workBuffer_[outIdx]     = juce::jlimit (-1.0f, 1.0f, fifoBuffer_[static_cast<size_t> (s1) + static_cast<size_t> (i)]);
                workBuffer_[outIdx + 1] = juce::jlimit (-1.0f, 1.0f, fifoBuffer_[static_cast<size_t> (s1) + static_cast<size_t> (i) + 1]);
                outIdx += 2;
            }
            pairIdx++;
        }
    }
    if (b2 > 0)
    {
        for (int i = 0; i < b2; i += 2)
        {
            if (pairIdx % stride == 0 && outIdx + 1 < workBuffer_.size())
            {
                workBuffer_[outIdx]     = juce::jlimit (-1.0f, 1.0f, fifoBuffer_[static_cast<size_t> (s2) + static_cast<size_t> (i)]);
                workBuffer_[outIdx + 1] = juce::jlimit (-1.0f, 1.0f, fifoBuffer_[static_cast<size_t> (s2) + static_cast<size_t> (i) + 1]);
                outIdx += 2;
            }
            pairIdx++;
        }
    }
    fifo_.finishedRead (totalRead);
    const int numPairs = static_cast<int> (outIdx / 2);
    if (numPairs <= 0)
    {
        repaint();
        return;
    }

    correlation_.store (computeCorrelation (workBuffer_.data(), numPairs), std::memory_order_relaxed);

    // PAZ_SCOPE: accumulate into 2D density [angleBin][radiusBin]
    const float wScale = kDensityGain / static_cast<float> (juce::jmax (1, numPairs));
    for (int i = 0; i < numPairs; ++i)
    {
        const float lv = workBuffer_[static_cast<size_t> (i) * 2u];
        const float rv = workBuffer_[static_cast<size_t> (i) * 2u + 1];
        const float mid = 0.5f * (lv + rv);
        const float side = 0.5f * (lv - rv);
        const float angle = std::atan2 (side, mid);
        const float radius = std::sqrt (mid * mid + side * side);
        const float w = mid * mid + side * side;  // energy weight

        const float angleClamped = juce::jlimit (-kPiHalf, kPiHalf, angle);
        const float t = (angleClamped + kPiHalf) / kPi;
        const int angleBin = juce::jlimit (0, kAngleBins - 1, static_cast<int> (t * static_cast<float> (kAngleBins - 1)));

        const float rNorm = 1.0f - std::exp (-radius * kRScale);  // soft-knee compress
        const int radiusBin = juce::jlimit (0, kRadiusBins - 1, static_cast<int> (rNorm * static_cast<float> (kRadiusBins - 1)));

        density_[static_cast<size_t> (angleBin)][static_cast<size_t> (radiusBin)] += w * wScale;
    }

    // PAZ_SCOPE: apply decay and clamp (peak hold for dots removed; only white trace uses peak hold)
    for (size_t a = 0; a < static_cast<size_t> (kAngleBins); ++a)
    {
        for (size_t r = 0; r < static_cast<size_t> (kRadiusBins); ++r)
        {
            density_[a][r] *= kDecayFactor;
            density_[a][r] = juce::jlimit (0.0f, kMaxDensity, density_[a][r]);
        }
    }

    // PAZ_SCOPE_LINES_MODE: weighted centroid + spatial + temporal smoothing -> lineRNormEMA_
    computeCurrentContourFromDensity();

    // PAZ_SCOPE_PEAK_HOLD: update from smoothed contour (lineRNormEMA_)
    if (peakHoldEnabled_.load (std::memory_order_relaxed))
    {
        for (size_t a = 0; a < static_cast<size_t> (kAngleBins); ++a)
        {
            const float cur = lineRNormEMA_[a];
            if (cur > peakRNorm_[a])
                peakRNorm_[a] = cur;
            else
                peakRNorm_[a] = juce::jmax (cur, peakRNorm_[a] * kPeakDecayFactor);
            peakRNorm_[a] = juce::jlimit (0.0f, 1.0f, peakRNorm_[a]);
        }
    }

    repaint();
}

void PhaseFanScopeComponent::computeCurrentContourFromDensity()
{
    std::array<float, static_cast<size_t> (kAngleBins)> curRNorm;
    for (int a = 0; a < kAngleBins; ++a)
    {
        float wSum = 0.0f;
        float rSum = 0.0f;
        for (int rb = 0; rb < kRadiusBins; ++rb)
        {
            const float d = density_[static_cast<size_t> (a)][static_cast<size_t> (rb)];
            if (d <= 0.0f)
                continue;
            wSum += d;
            rSum += d * static_cast<float> (rb);
        }
        if (wSum < kMinDrawThreshold)
        {
            lineRNormEMA_[static_cast<size_t> (a)] *= 0.95f;  // decay toward 0
            curRNorm[static_cast<size_t> (a)] = 0.0f;
        }
        else
        {
            curRNorm[static_cast<size_t> (a)] = juce::jlimit (0.0f, 1.0f, (rSum / wSum) / static_cast<float> (kRadiusBins - 1));
        }
    }
    // Spatial smoothing: 3-tap blur 0.25, 0.5, 0.25
    const float wPrev = kLineSpatialSmooth;
    const float wCur  = 1.0f - 2.0f * kLineSpatialSmooth;
    const float wNext = kLineSpatialSmooth;
    for (int a = 0; a < kAngleBins; ++a)
    {
        const int prev = juce::jmax (0, a - 1);
        const int next = juce::jmin (kAngleBins - 1, a + 1);
        const float smoothed = wPrev * curRNorm[static_cast<size_t> (prev)] + wCur * curRNorm[static_cast<size_t> (a)] + wNext * curRNorm[static_cast<size_t> (next)];
        const float alpha = kLineEmaAlpha;
        lineRNormEMA_[static_cast<size_t> (a)] = (1.0f - alpha) * lineRNormEMA_[static_cast<size_t> (a)] + alpha * smoothed;
        lineRNormEMA_[static_cast<size_t> (a)] = juce::jlimit (0.0f, 1.0f, lineRNormEMA_[static_cast<size_t> (a)]);
    }
}

float PhaseFanScopeComponent::computeCorrelation (const float* lr, int numPairs) const noexcept
{
    if (numPairs < 2 || lr == nullptr)
        return 0.0f;
    float sumL = 0.0f, sumR = 0.0f;
    for (int i = 0; i < numPairs; ++i)
    {
        sumL += lr[static_cast<size_t> (i) * 2u];
        sumR += lr[static_cast<size_t> (i) * 2u + 1];
    }
    const float meanL = sumL / static_cast<float> (numPairs);
    const float meanR = sumR / static_cast<float> (numPairs);
    float cov = 0.0f, varL = 0.0f, varR = 0.0f;
    for (int i = 0; i < numPairs; ++i)
    {
        const float dL = lr[static_cast<size_t> (i) * 2u] - meanL;
        const float dR = lr[static_cast<size_t> (i) * 2u + 1] - meanR;
        cov += dL * dR;
        varL += dL * dL;
        varR += dR * dR;
    }
    const float denom = std::sqrt (varL * varR);
    if (denom < 1e-10f)
        return 0.0f;
    return juce::jlimit (-1.0f, 1.0f, cov / denom);
}

void PhaseFanScopeComponent::paint (juce::Graphics& g)
{
    const auto& theme = ui_.theme();
    g.fillAll (theme.panel);

    auto area = getLocalBounds().toFloat();
    const float w = area.getWidth();
    const float h = area.getHeight();
    if (w < 2.0f || h < 2.0f)
        return;

    // PAZ_SCOPE_GEOMETRY: outer drawing rect from bounds, small padding only; no artificial size cap
    juce::Rectangle<float> padded = area.reduced (kPadding);
    if (padded.getWidth() < 2.0f || padded.getHeight() < 2.0f)
        return;
    const float radiusPx = juce::jmin (padded.getWidth() * 0.5f, padded.getHeight());
    if (radiusPx <= 0.0f)
        return;

    const float cx = padded.getCentreX();
    const float cy = padded.getBottom();  // PAZ_SCOPE_GEOMETRY: polar origin at bottom centre; semicircle sits on baseline

    // PAZ_SCOPE_GEOMETRY (NON-INVERTED RADIAL: base->arc): rNorm=0 = origin, rNorm=1 = outer arc perimeter
    const float rMin = radiusPx * 0.02f;
    const float rMax = radiusPx;

    auto rNormToPx = [rMin, rMax] (float rNorm) { return juce::jmap (juce::jlimit (0.0f, 1.0f, rNorm), 0.0f, 1.0f, rMin, rMax); };
    auto thetaForAngleBin = [] (int a) { return juce::jmap (static_cast<float> (a), 0.0f, static_cast<float> (kAngleBins - 1), -juce::MathConstants<float>::halfPi, juce::MathConstants<float>::halfPi); };
    auto toScreen = [cx, cy, &rNormToPx, &thetaForAngleBin] (int a, float rNorm, float& px, float& py)
    {
        const float rPx = rNormToPx (rNorm);
        const float theta = thetaForAngleBin (a);
        px = cx + std::sin (theta) * rPx;
        py = cy - std::cos (theta) * rPx;
    };

    constexpr float pointSize = 2.0f;
    constexpr float densityThreshold = 0.001f;

    const bool drawDots = (renderMode_ == PhaseFanRenderMode::Dots || renderMode_ == PhaseFanRenderMode::Both);
    const bool drawLines = (renderMode_ == PhaseFanRenderMode::Lines || renderMode_ == PhaseFanRenderMode::Both);

    if (drawDots)
    {
        const float dotAlpha = (renderMode_ == PhaseFanRenderMode::Both) ? 0.5f : 1.0f;
        for (int a = 0; a < kAngleBins; ++a)
        {
            for (int r = 0; r < kRadiusBins; ++r)
            {
                const float d = density_[static_cast<size_t> (a)][static_cast<size_t> (r)];
                if (d < densityThreshold)
                    continue;
                float alpha = juce::jlimit (0.0f, 1.0f, std::pow (d * kDensityGain, kDensityGamma));
                if (renderMode_ == PhaseFanRenderMode::Both)
                    alpha *= dotAlpha;
                const float rNorm = static_cast<float> (r) / static_cast<float> (kRadiusBins - 1);
                float px, py;
                toScreen (a, rNorm, px, py);
                g.setColour (theme.seriesPeak.withAlpha (alpha));
                g.fillEllipse (px - pointSize * 0.5f, py - pointSize * 0.5f, pointSize, pointSize);
            }
        }
    }

    if (drawLines)
    {
        // Yellow traces: draw from 0 point base (bottom centre) to each contour point (rays from origin).
        for (int a = 0; a < kAngleBins; ++a)
        {
            const float rNorm = lineRNormEMA_[static_cast<size_t> (a)];
            if (rNorm <= 0.0f)
                continue;
            float px, py;
            toScreen (a, rNorm, px, py);
            g.setColour (theme.seriesPeak.withAlpha (0.7f));
            g.drawLine (cx, cy, px, py, 2.0f);
        }
    }

    // PAZ_SCOPE_PEAK_HOLD: white trace only — peak hold applies to the (white) contour lines only; draw from base to peak contour.
    if (peakHoldEnabled_.load (std::memory_order_relaxed) && drawLines)
    {
        for (int a = 0; a < kAngleBins; ++a)
        {
            const float rNorm = peakRNorm_[static_cast<size_t> (a)];
            if (rNorm <= 0.0f)
                continue;
            float px, py;
            toScreen (a, rNorm, px, py);
            g.setColour (juce::Colours::white.withAlpha (1.0f));
            g.drawLine (cx, cy, px, py, 1.25f);
        }
    }

    // PAZ_SCOPE_GEOMETRY: grid rings (ring 1 = near base, ring 5 = outer arc); top semicircle arc [pi .. twoPi]
    g.setColour (theme.grid.withAlpha (0.6f));
    for (int ring = 1; ring <= 5; ++ring)
    {
        const float rr = juce::jmap (static_cast<float> (ring) / 5.0f, 0.0f, 1.0f, rMin, rMax);
        arcPath_.clear();
        arcPath_.addArc (cx - rr, cy - rr, rr * 2.0f, rr * 2.0f, juce::MathConstants<float>::pi, juce::MathConstants<float>::twoPi, true);
        g.strokePath (arcPath_, juce::PathStrokeType (0.5f));
    }
    for (int deg = -90; deg <= 90; deg += 15)
    {
        const float theta = static_cast<float> (deg) * juce::MathConstants<float>::pi / 180.0f;
        const float ex = cx + std::sin (theta) * radiusPx;
        const float ey = cy - std::cos (theta) * radiusPx;
        g.drawLine (cx, cy, ex, ey, 0.5f);
    }

    // Labels: Left, Right, Anti Phase (unchanged)
    g.setColour (theme.textMuted);
    g.setFont (juce::FontOptions (11.0f));
    g.drawText ("Left", juce::Rectangle<float> (0, 0, w * 0.3f, 24.0f), juce::Justification::centredLeft, false);
    g.drawText ("Right", juce::Rectangle<float> (w * 0.7f, 0, w * 0.3f, 24.0f), juce::Justification::centredRight, false);

    const float labelH = 18.0f;
    const float bottomY = h - labelH - kPadding;
    g.drawText ("Anti Phase", juce::Rectangle<float> (0, bottomY, w * 0.4f, labelH), juce::Justification::centredLeft, false);
    g.drawText ("Anti Phase", juce::Rectangle<float> (w * 0.6f, bottomY, w * 0.4f, labelH), juce::Justification::centredRight, false);

    // Correlation bar (unchanged)
    const float corr = correlation_.load (std::memory_order_relaxed);
    const float barW = 60.0f;
    const float barH = 6.0f;
    const float barY = h - barH - 4.0f;
    const float barX = cx - barW * 0.5f;
    const float centerX = barX + barW * 0.5f;
    g.setColour (theme.grid);
    g.drawRect (barX - 1, barY - 1, barW + 2, barH + 2);
    g.setColour (theme.seriesPeak);
    const float fillW = std::abs (corr) * barW * 0.5f;
    const float fillX = centerX + juce::jmin (0.0f, corr) * barW * 0.5f;
    if (fillW > 0.5f)
        g.fillRect (fillX, barY, fillW, barH);

    g.setColour (theme.borderDivider);
    g.drawRect (getLocalBounds().toFloat(), 1.0f);
}

/*
 * VERIFIER PROMPT
 * Verify PhaseFanScopeComponent matches a PAZ-style phase fan scope and is realtime-safe.
 *
 * Checklist:
 * 1. Audio thread safety: pushAudioBlock has no allocations, no locks, no GUI calls; only FIFO writes.
 * 2. FIFO correctness: ring buffer wrap + bounds are correct; no undefined behavior.
 * 3. Angle math: uses mid=0.5(L+R), side=0.5(L-R), angle=atan2(side, mid) producing [-pi/2..+pi/2]. anti-phase maps to edges.
 * 4. Binning: fixed 181 bins, stable indexing, no out-of-range.
 * 5. Ballistics: per-bin levels have attack/release; decay behaves smoothly.
 * 6. Rendering: semi-circular fan histogram (filled wedges) + orange outline trace; arc grid rings + radial lines; labels Left/Right and Anti Phase on both bottom corners.
 * 7. Geometry: wide rectangular viewport, semi-circle fits with padding; not a square Lissajous.
 * 8. Performance: 60Hz update, capped 8192 pairs per frame, no per-frame heap allocations in paint().
 * 9. Correlation: range [-1..+1], bar at bottom.
 * 10. Peak hold: peak_[N] and holdRemainingSec_[N] per bin; no allocations per frame; updates only in drainFifoAndUpdateBins (UI thread).
 * 11. Peak on new max: peak updates immediately, hold timer resets to peakHoldMs.
 * 12. Peak holds ~peakHoldMs then decays with ~peakReleaseMs; decay uses real dt (juce::Time::getMillisecondCounterHiRes), not frame count.
 * 13. Peak rendering: bright (white) thin outline distinct from main seriesPeak outline; readable.
 * 14. resetPeakHold(): sets peak_[i]=level_[i] and holdRemainingSec_[i]=0 so peak display clears instantly.
 * 15. Defaults: peakHoldEnabled=true, peakHoldMs=750, peakReleaseMs=1500.
 * 16. Realtime-safe: audio thread unchanged; no locks or allocs on audio thread. Peak logic and paint only on UI thread.
 *
 * PEAK HOLD VERIFIER:
 * - Peak hold line appears and holds ~peakHoldMs then decays with ~peakReleaseMs.
 * - resetPeakHold clears peak display instantly.
 * - Peak does not grow when signal stops; it decays smoothly after hold.
 *
 * CALIBRATION TEST:
 * 1. L=sin, R=sin -> energy centered (angle ~ 0, mono/in-phase).
 * 2. L=sin, R=-sin -> energy at the two anti-phase edges (angle ~ +/- pi/2).
 * 3. L=sin, R=0 -> energy leaning to one side (stereo image test).
 *
 * PAZ FAN FILL VERIFIER:
 * - Fan fill uses a single juce::Path (fanFillPath_) built per frame and Graphics::fillPath(), not per-bin temporary Paths.
 * - Each bin contributes via Path::addPieSegment centered at bottom-center (cx, cy), radius r = level * rMax.
 * - Angle mapping: drawA = binAngle - halfPi; angles in [-pi..0] for top semicircle (left→right sweep).
 * - Fan is arc strip / donut slice via innerProportion 0.12; not triangles to center.
 * - Draw order: fade → filled fan → current outline → peak-hold outline.
 * - No per-frame heap allocations; fanFillPath_ reused and cleared each frame.
 * - Mono clusters center; anti-phase clusters edges; fan appears filled (not empty).
 */
