FILES EDITED

melechdsp-hq/shared/mdsp_ui/include/mdsp_ui/rta/RTADisplayModel.h
  - Added getCachedPeakMaxPosition(float&, float&, float&) const; and isCachedPeakValid() const.
  - Added member cachedPeakValid_ (bool).

melechdsp-hq/shared/mdsp_ui/src/rta/RTADisplayModel.cpp
  - invalidatePaths(): set cachedPeakValid_ = false so cache is cleared on reset/config change.
  - buildFftPaths(): At start, clear peak path/aux only when not (isHoldOn && cachedPeakValid_). When isHoldOn, build cachedPeakPath_ and aux from fftPeakHoldDb when it has data; otherwise keep existing cache. When !isHoldOn, keep previous behavior (build from fftPeakDb when hasPeakSignal, else clear). Build cachedPeakHoldPath_ only when !isHoldOn so hold path is not duplicated when peak path is hold.
  - getCachedPeakMaxPosition(): New; finds argmax in cachedPeakDbUnclamped_, returns corresponding x, y, db from cached arrays; returns false if cache invalid or empty.

melechdsp-hq/shared/mdsp_ui/src/rta/RTADisplayRenderer.cpp
  - Session marker (both sites): When s.isHoldOn and model.getCachedPeakMaxPosition(x,y,db) returns true, draw marker at (x,y); otherwise use sessionMarkerBin/sessionMarkerDb for x,y.
  - FFT crosshair/label: Label already uses peakDbTrue from getDecimatedPeakAtX or fallback; no change to source of db. Added labelSrc debug behind MDSP_DEBUG_CROSSHAIR only: "labelSrc=HOLD", "labelSrc=LIVE", or "labelSrc=FALLBACK".

AnalyzerPro/Source/ui/analyzer/AnalyzerDisplayView.cpp
  - Uncommented rtaDisplay.setHoldStatus(isHoldOn_) so model state has isHoldOn and buildFftPaths/renderer can branch on Peak Hold.

REASONING

Peak trace visibility when Peak Hold is on: buildFftPaths no longer clears the peak cache on silence when isHoldOn and cachedPeakValid_; it keeps the last built path so the peak trace remains visible after stopping signal.

Hover label: Single FFT hover path already uses getDecimatedPeakAtX for Y and peakDbTrue for the label. With the cache preserved when hold on, getDecimatedPeakAtX returns the held decimated peak at x, so the label shows the held value. Fallback remains bin-based when cache is empty.

Global max marker: Session marker is drawn at the max of the cached peak curve when Peak Hold is on (getCachedPeakMaxPosition), so the yellow mark sits on the held peak; when hold is off, marker uses existing sessionMarkerBin/sessionMarkerDb.

Debug logs: labelSrc is printed only when MDSP_DEBUG_CROSSHAIR is 1; no production logs added.
