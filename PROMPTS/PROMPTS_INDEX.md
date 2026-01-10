# PROMPTS_INDEX — AnalyzerPro / MelechDSP
Short feature → prompt mapping (descriptions only).

## Engine / RT-critical
- **Standalone macOS loopback routing** → `01_ENGINE/00_macos_loopback_routing_standalone.txt`
- **RT-safe FFT resize (no audio-thread allocs)** → `01_ENGINE/01_rt_safe_fft_resize.txt`
- **Deferred FFT request/apply (message-thread realloc)** → `01_ENGINE/02_deferred_fft_request_apply.txt`
- **Peak/Hold model cleanup (Freeze vs Hold separation)** → `01_ENGINE/03_peak_hold_model_cleanup.txt`
- **Reset peaks (engine-only)** → `01_ENGINE/04_reset_peaks_engine_only.txt`
- **Peak decay models (dB/sec vs time-constant)** → `01_ENGINE/05_peak_decay_models_db_vs_tc.txt`
- **Processor FFT param uses request-only** → `01_ENGINE/06_processor_fft_param_request_only.txt`

## UI Analyzer (visualization only)
- **FFT bin count contract** → `02_UI_ANALYZER/01_fft_bin_count_contract.txt`
- **FFT display fix (use fftBinCount)** → `02_UI_ANALYZER/02_fft_display_fix_use_fftBinCount.txt`
- **User-selectable dB range (-60/-90/-120)** → `02_UI_ANALYZER/03_db_range_user_selectable.txt`
- **Independent FFT vs Peak ranges** → `02_UI_ANALYZER/04_independent_fft_vs_peak_range.txt`
- **Peak flash feedback (UI-only)** → `02_UI_ANALYZER/05_peak_flash_feedback_ui_only.txt`
- **Extract peak invalid sentinel constant** → `02_UI_ANALYZER/06_extract_peak_sentinel_constant.txt`
- **Animate dB range transitions** → `02_UI_ANALYZER/07_animate_db_range_transitions.txt`

## UI Controls
- **HeaderBar reset peaks button** → `03_UI_CONTROLS/01_headerbar_reset_peaks_button.txt`
- **Reset peaks keyboard shortcut** → `03_UI_CONTROLS/02_reset_peaks_keyboard_shortcut.txt`
- **Persist dB range via APVTS** → `03_UI_CONTROLS/03_db_range_persistence_apvts.txt`
- **HeaderBar layout polish / clipping fixes** → `03_UI_CONTROLS/04_headerbar_layout_polish.txt`

## Performance
- **Apply analyzer params once (avoid setter spam)** → `04_PERFORMANCE/01_apply_analyzer_params_once.txt`
- **Cache APVTS raw pointers** → `04_PERFORMANCE/02_cache_apvts_raw_pointers.txt`
- **Release-safe null guards (RT-safe)** → `04_PERFORMANCE/03_guard_null_params_release.txt`
- **Reduce processBlock churn** → `04_PERFORMANCE/04_reduce_processBlock_churn.txt`

## Refactor Guards
- **No RT allocations rules** → `05_REFACTOR_GUARDS/01_no_rt_allocations_rules.txt`
- **No C++ class redefinition / file rewrite** → `05_REFACTOR_GUARDS/02_no_cpp_class_redefinition.txt`
- **UI owns mapping, engine owns math** → `05_REFACTOR_GUARDS/03_ui_owns_mapping_engine_owns_math.txt`

