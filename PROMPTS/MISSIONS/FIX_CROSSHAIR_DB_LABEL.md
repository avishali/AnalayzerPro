Implemented. FFT crosshair label now shows the true decimated peak dB (unclamped). Marker Y unchanged (still from decimated path geometry).

Files changed:
  melechdsp-hq/shared/mdsp_ui/src/rta/RTADisplayRenderer.cpp — paintFFTMode and paintInteractionOverlays: get crosshair Y and peak dB from model.getDecimatedPeakAtX(xPlotPx, crosshairY, &peakDbTrue). Use peakDbTrue for label text; build readout as getFftHoverFreqText() + "  " + formatDb(peakDbTrue) (or "  —" if no value). Measure width with GlyphArrangement. Fallback when getDecimatedPeakAtX fails: use peakDbTraceAtBin for both Y and label.
  melechdsp-hq/shared/mdsp_ui/include/mdsp_ui/rta/RTADisplayModel.h — added cachedPeakDbUnclamped_; buildDecimatedPath now takes optional outDbUnclamped.
  melechdsp-hq/shared/mdsp_ui/src/rta/RTADisplayModel.cpp — buildDecimatedPath: store raw dB before jlimit in outDbUnclamped when provided; peak path build passes &cachedPeakDbUnclamped_; clear cachedPeakDbUnclamped_ with other peak caches; getDecimatedPeakAtX returns cachedPeakDbUnclamped_[idx] when available so label gets unclamped value.

Result: marker stays at (possibly clamped) trace Y; label shows true peak e.g. -138.6 dB instead of clamped -120 dB.
