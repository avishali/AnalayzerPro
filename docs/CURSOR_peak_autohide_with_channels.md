# Cursor task — auto-hide the combined Peak (and its hold) when any channel trace is on

Decision: the combined-spectrum Peak trace + its peak-hold ceiling should show ONLY when the view is
the combined spectrum (RMS and/or Peak alone). As soon as ANY individual channel trace is enabled —
L, R, Mid, Side, Mono, or Stereo(LR) — the combined Peak and its hold auto-hide (they overlay the
channel traces and clutter). RMS does NOT count as a channel (RMS + Peak must still show Peak). This
must apply on BOTH the Metal path and the CPU/bridge path so Mac == Windows.

All edits in `Source/ui/analyzer/AnalyzerDisplayView.{h,cpp}`.

## 1. Add a small helper — `AnalyzerDisplayView.h`
Next to the other private members (near `bool showPeak_ = true;`), add:
```cpp
// True when any individual channel trace is shown (L/R/Mid/Side/Mono/Stereo) — excludes RMS.
// Used to auto-hide the combined-spectrum Peak + peak-hold so they don't clutter channel views.
bool anyChannelTraceEnabled() const noexcept
{
    return traceConfig_.showSide || traceConfig_.showMid || traceConfig_.showL
        || traceConfig_.showR || traceConfig_.showLR || traceConfig_.showMono;
}
```

## 2. Metal frame builder — `AnalyzerDisplayView.cpp` (~lines 617-635)
Gate the Peak and Peak-hold visibility on the channel check.

- Peak trace (~line 620): change the visible arg from
  ```cpp
                      showPeak_,
  ```
  to
  ```cpp
                      showPeak_ && ! anyChannelTraceEnabled(),
  ```
- Peak-hold trace (~line 634): change
  ```cpp
                  showPeak_ && isHoldOn_);
  ```
  to
  ```cpp
                  showPeak_ && isHoldOn_ && ! anyChannelTraceEnabled());
  ```

## 3. CPU / bridge feed — `AnalyzerDisplayView.cpp` (~line 1452, the `setFFTData` call)
Currently feeds peak / peak-hold when `usePeaks && showPeak_`. Also require no channel trace:
```cpp
analyzerBridgeWidget_.setFFTData (fftFrame_.display_,
                                  (usePeaks && showPeak_ && ! anyChannelTraceEnabled()) ? &fftPeakDbDisplay_ : nullptr,
                                  (showPeak_ && ! anyChannelTraceEnabled() && ! peakHoldDb.empty()
                                       && renderStateProvider_.usePeakHold()) ? &peakHoldDb : nullptr);
```
(Keep the existing structure; just add the `&& ! anyChannelTraceEnabled()` term to both the peak and
peak-hold operands.)

## Acceptance (Claude builds/signs/installs; human verifies in PT)
- Peak on, no channel traces (RMS only, or nothing) → combined Peak (and hold ceiling when Hold on)
  visible — Peak-primary workflow unchanged.
- Enable any channel (L/R/M/S/Mono/Stereo) → the combined Peak AND its hold ceiling disappear; only
  the channel traces (and RMS if on) remain. Disable all channels → Peak returns.
- No crash; never-blank guard and peak-hold latch behavior unchanged.
