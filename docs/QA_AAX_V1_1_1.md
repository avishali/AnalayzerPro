# AnalyzerPro v1.1.1 AAX QA Report

## Scope

Validation of the AAX build for `AnalyzerPro` version `1.1.1` in Pro Tools Developer environment on macOS.

## Result Summary

- Overall AAX validation status: **PASS**

## Test Outcomes

- Discovery and instantiate: **PASS**
  - Plugin appears in Pro Tools insert menu.
  - Plugin instantiates and UI opens correctly.

- Audio path and bypass behavior: **PASS**
  - Audio pass-through is stable.
  - Bypass behavior is correct.

- Automation behavior: **PASS**
  - Parameter automation write/read path works.

- Runtime stability under host changes: **PASS**
  - Stable through sample-rate and buffer-size changes.

- Multi-instance stability: **PASS**
  - Multiple instances run without host/plugin instability.

- Plugin preset save/load in Pro Tools: **PASS**
  - Plugin-level preset persistence functions correctly.

- Full Pro Tools session save/reopen recall: **N/A (Host Limitation)**
  - In Pro Tools Developer mode, full session save is disabled.
  - This is a host environment constraint, not a plugin defect.

## Release Readiness Note

From functional AAX behavior, `AnalyzerPro v1.1.1` is release-ready.
Remaining release gates are operational/compliance tasks:

- Final signing pipeline completion (Apple Developer ID / Windows Authenticode).
- AAX distribution signing/compliance workflow (PACE/Avid process).
- One final state-recall confirmation in a Pro Tools environment where full session save/reopen is enabled.
