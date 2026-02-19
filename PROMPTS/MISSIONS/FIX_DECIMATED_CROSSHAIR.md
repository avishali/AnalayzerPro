FILES AND FUNCTIONS TO CHANGE

1) melechdsp-hq/shared/mdsp_ui/include/mdsp_ui/rta/RTADisplayModel.h
   Add private members after cached paths (e.g. after cachedLogPeakPath_): std::vector<float> cachedPeakXPlotPx_; std::vector<float> cachedPeakYPlotPx_; std::vector<float> cachedPeakDb_;
   Add public declaration after path accessors: bool getDecimatedPeakAtX(float xPlotPx, float& outYPlotPx, float* outDbOpt) const;
   Add optional output parameters to buildDecimatedPath declaration: three std::vector<float>* (outXPlotPx, outYPlotPx, outDb), default nullptr.

2) melechdsp-hq/shared/mdsp_ui/src/rta/RTADisplayModel.cpp
   buildFftPaths: In the block where hasPeakSignal is true and buildDecimatedPath(state_.fftPeakDb, cachedPeakPath_, ...) is called, add the three optional arguments &cachedPeakXPlotPx_, &cachedPeakYPlotPx_, &cachedPeakDb_. In the same block when hasPeakSignal is false (else branch that does cachedPeakPath_.clear()), also clear the three cached peak vectors.
   buildDecimatedPath: Add the three optional parameters. At start, if any of the three pointers is non-null, clear those vectors. In the for (int x = 0; x <= w; ++x) loop, where pts.emplace_back(x0, y) is called, if the three output pointers are non-null push x0 to outXPlotPx, y to outYPlotPx, finalDb to outDb. After the smoothing block (the for (size_t i = 1; i < pts.size() - 1; ++i) that updates pts[i].y), if outYPlotPx is non-null and its size equals pts.size(), overwrite (*outYPlotPx)[i] = pts[i].y for all i.
   Implement getDecimatedPeakAtX: If cachedPeakXPlotPx_ is empty return false. Binary search (or std::lower_bound) on cachedPeakXPlotPx_ for xPlotPx; pick nearest of the two candidates around the insertion point; clamp index to [0, size-1]. Set outYPlotPx = cachedPeakYPlotPx_[idx]. If outDbOpt non-null set *outDbOpt = cachedPeakDb_[idx]. Return true.

3) melechdsp-hq/shared/mdsp_ui/include/mdsp_ui/rta/RTADisplayRenderer.h
   paintInteractionOverlays: Add parameter const RTADisplayModel& model (e.g. after RenderState& s or before fftCrosshairAlreadyDrawn).

4) melechdsp-hq/shared/mdsp_ui/src/rta/RTADisplayRenderer.cpp
   paint: At both call sites of paintInteractionOverlays add the model argument (e.g. paintInteractionOverlays(..., model) so the new parameter is passed).
   paintFFTMode: In the FFT crosshair block where crosshairY is computed (current logic uses binIdx, freqHz, peakDbTraceAtBin, dbToYWithCompensation): Compute xPlotPx = crosshairX. Call model.getDecimatedPeakAtX(xPlotPx, crosshairY, &dbOpt) with a local float dbOpt. If it returns true, use the returned crosshairY (and optionally dbOpt for label text). Else keep existing fallback (bin-based peakDbTraceAtBin and dbToYWithCompensation). Do not allocate in paint; getDecimatedPeakAtX must not allocate.
   paintInteractionOverlays: Add const RTADisplayModel& model to the function definition. In the FFT crosshair block, same change as paintFFTMode: compute xPlotPx = crosshairX; if model.getDecimatedPeakAtX(xPlotPx, crosshairY, &dbOpt) use it, else keep current bin-based Y fallback.

5) Double-crosshair: No code change required. paintInteractionOverlays is already called with fftCrosshairAlreadyDrawn = (s.viewMode == 0) so FFT crosshair is drawn only once (in paintFFTMode when viewMode is 0). Both paths use the same decimated query once the above changes are in place.

6) AnalyzerPro path: No changes in AnalyzerPro/Source. Both HQ AnalyzerComponent and AnalyzerPro use RTADisplayRenderer from melechdsp-hq; a single crosshair draw and the same getDecimatedPeakAtX usage in the renderer covers both.

7) Debug: Remove or gate behind MDSP_DEBUG_CROSSHAIR any crosshair debug logs in RTADisplayRenderer (paintFFTMode and paintInteractionOverlays). Set or leave MDSP_DEBUG_CROSSHAIR default OFF in the define at top of RTADisplayRenderer.cpp.
