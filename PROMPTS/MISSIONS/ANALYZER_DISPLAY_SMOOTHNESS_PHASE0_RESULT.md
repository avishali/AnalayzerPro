# ANALYZER_DISPLAY_SMOOTHNESS_V1 — Phase 0 Result

Date: 2026-05-31
Build: build-release-dev (Release, PLUGIN_DEV_MODE=1, universal). HUD fixed in commit 13f98eb (runtime wrapper label + non-truncated multi-line draw).
Method: live HUD read on the dev build, steady audio, settled ~10s. Window scale=1.00 in both.

## Runtime HUD Measurements (captured)

| metric | VST3 (in DAW) | Standalone | read |
|---|---|---|---|
| paint/s | 29 | 30 | both at target, regular |
| paint_ms(last) | 1.77 | 2.81 | both cheap — render cost NOT the bottleneck |
| timer_jitter_avg_ms | 1.692 | 1.378 | both low (~5% of 33ms period) — cadence regular |
| actual_fps | 28.7 | 29.2 | both slightly under 30 |
| actual_timer_ms_avg | 34.90 | 34.20 | both ~34ms vs 33.33 expected |
| timer_late_cnt/s | 0 | 0 | timer not starved |
| spec_resized/s | 0 | 0 | no resize churn |
| pump_throttle/s, pump_reject/s | 0 / 0 | 0 / 0 | data flow clean |

AAX: not measured — the dev build's auto-install (COPY_PLUGIN_AFTER_BUILD, CMakeLists.txt:299) overwrote the signed system AAX with an adhoc/unsigned one (codesign: Signature=adhoc), so Pro Tools won't load it. Not required for the hypothesis.

## Conclusion

NEITHER cadence/jitter NOR render-cost is the bottleneck. VST3 and Standalone are statistically identical: low jitter (~1.4–1.7ms), cheap paint (~2ms), regular ~29fps, zero throttling/late ticks in both.

=> The Phase 1 premise (irregular in-host repaint cadence from three unsynchronized 30Hz clocks) is NOT supported by the data. Do NOT proceed to Phase 1 on this evidence.

Two implications:
1. The original "VST3/AAX steppy, Standalone fine" report was most likely an artifact of the duplicate-install hazard — comparing a STALE VST3 build against a CURRENT Standalone. With matched 1.1.1 builds the metrics match. NEEDS owner re-confirmation by eye with the matched builds.
2. Any residual steppiness is the inherent limit of 30fps with NO inter-frame interpolation of trace geometry, compounded by ~29fps (non-integer) cadence on a 60Hz+ display → periodic 2-vs-3 refresh judder. Affects ALL formats equally.

## Recommendation

- Do NOT do Phase 1 (collapsing clocks); jitter already low — no perceptible win, and it spends the CPU-first budget for nothing.
- Owner eye-check with the matched builds:
  - If VST3 no longer looks worse than Standalone → original issue was the stale-build mismatch (now fixed). Remaining steppiness is the universal 30fps limit.
  - If VST3 STILL looks worse → a real format difference these UI-thread metrics don't capture (host NSView compositing / vsync alignment) → investigate separately.
- For genuinely "glassy" motion in ALL formats, the lever is PHASE 2: drive repaint from juce::VBlankAttachment at the display's native rate (60/120Hz) and interpolate trace geometry between the 30Hz data frames. paint_ms ~2ms leaves headroom. Requires owner sign-off (CPU-first).
- Side note: actual_fps ~29 (timer_ms_avg ~34 vs 33.33 expected) — the juce::Timer runs slightly slow. Bumping to exactly 30 would not fix steppiness; only higher-rate + interpolation (Phase 2) does.

## Owner eye-check + decision (2026-05-31)

- Eye-check with matched dev builds: VST3 and Standalone now look THE SAME. => The original "VST3/AAX steppy, Standalone fine" report WAS the stale-build/duplicate-install mismatch (comparing an old VST3 against a current Standalone). RESOLVED by the clean matched 1.1.1 install. No format-specific cadence problem exists.
- Direction chosen: pursue PHASE 2 (glassy motion) for absolute smoothness in ALL formats — VBlank render at display native rate + inter-frame interpolation of trace geometry. Data pump stays 30Hz (no DSP cost). See RUNBOOKS/ANALYZER_DISPLAY_GLASSY_MOTION_V2.md.

PHASE 0 CLOSED. Phase 1 (single-clock cadence fix) is NOT pursued — unjustified by data. Proceeding to Phase 2.
