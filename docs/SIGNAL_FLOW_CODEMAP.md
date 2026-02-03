# AnalyzerPro Snapshot Pipeline — Code Map

## Verification (code-accurate)

- **Function existence**: All functions referenced below exist in the stated files (AnalyzerEngine.cpp/.h, AnalyzerSnapshot.h, AnalyzerDisplayView.cpp/.h, RTADisplay.cpp, PluginProcessor.cpp). Processor class name in code: `AnalayzerProAudioProcessor` (PluginProcessor.cpp).
- **Snapshot production path**: `stagingSnapshot_` is filled in `AnalyzerEngine::computeFFT()` (ref to member `stagingSnapshot_`), then `publishSnapshot(snapshot)` is called with that reference; `publishSnapshot` copies into `published_.data` and increments `published_.sequence` (PublishedAnalyzerSnapshot). Path: **stagingSnapshot_ fill → publishSnapshot(stagingSnapshot_) → published_.data + published_.sequence**.
- **UI consumption path**: **AnalyzerDisplayView::timerCallback** → **AnalyzerEngine::getLatestSnapshot(snapshot_)** → if valid, **AnalyzerDisplayView::updateFromSnapshot(snapshot_)** → **rtaDisplay.setFftMeta** / **rtaDisplay.setFFTData** / **setBandData** / **setLogData** / **setMultiTraceData** (and optionally **setSessionMarker**). All in Source/ui/analyzer/AnalyzerDisplayView.cpp and RTADisplay.cpp.

## File paths (confirmed)

- `Source/analyzer/AnalyzerEngine.cpp`
- `Source/analyzer/AnalyzerEngine.h`
- `Source/analyzer/AnalyzerSnapshot.h`
- `Source/ui/analyzer/AnalyzerDisplayView.cpp`
- `Source/ui/analyzer/AnalyzerDisplayView.h`
- `Source/ui/analyzer/rta1_import/RTADisplay.cpp`
- `Source/PluginProcessor.cpp`

---

## AnalyzerEngine (Source/analyzer/AnalyzerEngine.cpp, .h)

| Item | Detail |
|------|--------|
| **Thread** | Audio thread: `processBlock`, `computeFFT`, `publishSnapshot`. Message thread: `applyPendingFftSizeIfNeeded` (only when FFT resize is applied). |
| **Key functions** | `prepare()` — init FFT, set `published_.sequence` to 1 if 0, set `published_.data.isValid = false`. `processBlock()` — fill FIFOs, when `samplesCollected >= currentHopSize` call `computeFFT()` (and multi-trace L/R FFTs then mono `computeFFT()`). `computeFFT()` — window, FFT, magnitudes, smoothing, ballistics, peak hold, fill `stagingSnapshot_`, call `publishSnapshot(stagingSnapshot_)`. `publishSnapshot(source)` — copy source into `published_.data`, increment `published_.sequence`, set `hasNewData_ = true`. `getLatestSnapshot(dest)` — seqlock-style read of `published_.data` (retry up to 3 times if sequence changes during copy). `getFFTData()` — return `published_.data.fftDb.data()`. `hasNextDataBlock()` / `clearDataFlag()` — read/clear `hasNewData_`. `requestFftSize(fftSize)` — set `pendingFftSize_`, `fftResizeRequested_`, invalidate `published_.data.isValid`. `applyPendingFftSizeIfNeeded()` — on message thread, call `initializeFFT(requested)` and clear pending flags. |
| **Produces** | `AnalyzerSnapshot` filled in `stagingSnapshot_` inside `computeFFT()`; then copied into `published_.data` and exposed via `published_.sequence` / `hasNewData_`. |
| **Consumes** | Audio buffer (from `processBlock`); APVTS-driven params applied in `PluginProcessor::processBlock` (FFT size, hold, decay, smoothing). |

---

## AnalyzerSnapshot (Source/analyzer/AnalyzerSnapshot.h)

| Item | Detail |
|------|--------|
| **Thread** | Data structure only; no thread. |
| **Key types** | `AnalyzerSnapshot` — trivially copyable struct: `fftDb`, `fftPeakDb`, `fftPeakHoldDb`, `powerL`, `powerR`, `fftBinCount`, `fftSize`, `sampleRate`, `isValid`, `isHoldOn`, `multiTraceEnabled`, etc. `PublishedAnalyzerSnapshot` — `std::atomic<uint32_t> sequence{0}` and `AnalyzerSnapshot data`. |
| **Produces** | N/A (definition only). |
| **Consumes** | N/A. |

---

## “New snapshot” indicator

- **Atomic sequence counter**: `PublishedAnalyzerSnapshot::sequence` (`published_.sequence` in `AnalyzerEngine`).
- **Where written**: `AnalyzerEngine::publishSnapshot()` — after copying source into `published_.data`, does `currentSeq = published_.sequence.load(relaxed)`, `next = (currentSeq == 0) ? 1 : (currentSeq + 1)`, `published_.sequence.store(next, release)`.
- **Where read**: `AnalyzerEngine::getLatestSnapshot()` — loads `published_.sequence` with `memory_order_acquire` (seq1 before copy, seq2 after copy); returns true only if `seq1 == seq2 && seq1 != 0` (stable read). Optional: `hasNewData_` is set in `publishSnapshot()` and can be read by UI via `hasNextDataBlock()`; it is not required for the codemap and is not used in the current UI read path (UI uses `getLatestSnapshot` only).

---

## AnalyzerDisplayView (Source/ui/analyzer/AnalyzerDisplayView.cpp, .h)

| Item | Detail |
|------|--------|
| **Thread** | UI / message thread (JUCE Timer). |
| **Key functions** | `timerCallback()` — 60 Hz; reads APVTS trace config, calls `audioProcessor.getAnalyzerEngine().applyPendingFftSizeIfNeeded()`, then `getAnalyzerEngine().getLatestSnapshot(snapshot_)`; if `gotSnapshot` and `snapshot_.isValid && fftBinCount > 0`, sets `lastValidSnapshot_ = snapshot_`, `hasLastValid_ = true`, and calls `updateFromSnapshot(snapshot_)`. `updateFromSnapshot(snapshot)` — sets `rtaDisplay.setFftMeta()`, copies snapshot into member vectors, applies weighting and ballistics, then by mode calls `rtaDisplay.setFFTData()` / `setBandData()` / `setLogData()` and optionally `setMultiTraceData()`. |
| **Produces** | FFT/Band/Log vectors and display state for `RTADisplay`; no snapshot. |
| **Consumes** | `AnalyzerSnapshot` from `getLatestSnapshot(snapshot_)`; APVTS for trace config, weighting, release, smoothing. |

---

## RTADisplay (Source/ui/analyzer/rta1_import/RTADisplay.cpp)

| Item | Detail |
|------|--------|
| **Thread** | UI / message thread (setters and `paint` called from message thread). |
| **Key functions** | `setFFTData(fftBinsDb, peakBinsDbNullable, peakHoldBinsDbNullable)` — store in `state`, set `state.status = Ok`, `invalidatePaths()`, `repaint()`. `setBandData()`, `setLogData()`, `setFftMeta()`, `setDbRange()`, `setTraceConfig()`, `setMultiTraceData()` — update state and repaint. No internal timer; all updates are driven by `AnalyzerDisplayView::timerCallback()` calling these setters. `paint()` — dispatches to `paintFFTMode` / `paintBandsMode` / `paintLogMode`; FFT mode uses `buildFftPaths()` (paths built from `state.fftDb`, `state.fftPeakDb`, `state.fftPeakHoldDb`, multi-trace). |
| **Produces** | Rendered frame (paint only). |
| **Consumes** | Vectors and meta passed in by `AnalyzerDisplayView::updateFromSnapshot()` (and related setters from timerCallback for dB range animation, trace config). |

---

## PluginProcessor (Source/PluginProcessor.cpp)

| Item | Detail |
|------|--------|
| **Thread** | Audio thread for `processBlock`; `prepareToPlay` / `releaseResources` from host. |
| **Key functions** | `prepareToPlay()` — `analyzerEngine.prepare(sampleRate, samplesPerBlock)`, `analyzerEngine.setPeakHoldMode(Off)`. `processBlock()` — copy input to `analysisBuffer`, push to `spectrumBufferQueue_`, apply gain to output buffer, read APVTS and call `analyzerEngine.requestFftSize()`, `setSmoothingOctaves()`, `setHold()`, `setReleaseTimeMs()`; if not bypassed, call `analyzerEngine.processBlock(analysisBuffer)`. No timer in processor; snapshot pipeline is entirely driven by audio → engine → publish and UI timer → getLatestSnapshot → updateFromSnapshot → RTADisplay setters. |
| **Produces** | Audio output; feeds `AnalyzerEngine` with input and parameter changes. |
| **Consumes** | Input buffer; APVTS. |

---

## Summary: data produced/consumed

| Component | Produces | Consumes |
|-----------|----------|----------|
| **AnalyzerEngine** | `PublishedAnalyzerSnapshot` (sequence + data), `hasNewData_` | Audio buffer, FFT/smoothing/hold/decay params |
| **AnalyzerSnapshot.h** | (types only) | — |
| **AnalyzerDisplayView** | RTADisplay state (setFFTData/setBandData/setLogData/setFftMeta/setMultiTraceData) | `AnalyzerSnapshot` from `getLatestSnapshot`, APVTS |
| **RTADisplay** | Rendered pixels | State from AnalyzerDisplayView setters |
| **PluginProcessor** | Audio out, analyzer input + param updates | Input buffer, APVTS |

---

## Decay / animation without new snapshots

- **AnalyzerDisplayView::timerCallback** (same 60 Hz timer): When no new snapshot is available (`!gotSnapshot` or `!snapshot_.isValid`), the timer still runs **minDbAnim_.getNextValue()** and **rtaDisplay.setDbRange(0.0f, minDb)** each tick, so dB range animation advances and triggers **repaint()** when `minDbAnim_.isSmoothing()`. When **(minDbAnim_.isSmoothing() || peakScaleDirty_ || flashActive) && hasLastValid_** is true, the same timer re-feeds last valid data (**fftDb_**, **fftPeakDb_**, **peakHoldDbDisplay_**, etc.) to **rtaDisplay.setFFTData** / **setBandData** / **setLogData** without calling **getLatestSnapshot** or **updateFromSnapshot** for that tick. So visuals (dB range, peak flash, peak remap) can advance without a new engine snapshot.
- **RTADisplay**: No internal `Timer` or `timerCallback`. **paint()** (and thus **paintFFTMode** / **drawSilkTrace**) uses **juce::Time::getMillisecondCounterHiRes()** for peak-trace shimmer (time-based alpha modulation). Each repaint can show a slightly different highlight on the same path data; no data decay inside RTADisplay.
- **PluginProcessor**: **StandalonePersistence** (standalone build only) uses a one-shot **Timer** (10 ms) for init only; it does not drive analyzer or display.
