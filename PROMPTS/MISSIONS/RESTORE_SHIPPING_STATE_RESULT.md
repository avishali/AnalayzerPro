MISSION_ID: RESTORE_SHIPPING_STATE_V1_1_1

TITLE
Restore shipping state result: clean 1.1.1 release installed, AAX PACE-signed.

BUILD
- Build dir: build-release
- Build exit: 0
- Version: 1.1.1
- Formats: AU, VST3, AAX, Standalone
- Dev Mode: OFF / 0
- Release build configured with PLUGIN_DEV_MODE=OFF; diagnostics HUD compile path is disabled for the installed shipping copy.

VERSION / ARCH / DATE
| Format | Installed path | CFBundleShortVersionString | CFBundleVersion | Arch | Bundle date | Binary date |
|---|---|---:|---:|---|---|---|
| VST3 | ~/Library/Audio/Plug-Ins/VST3/AnalyzerPro.vst3 | 1.1.1 | 1.1.1 | x86_64 arm64 | 2026-06-01 00:06:26 | 2026-06-01 00:13:38 |
| AU | ~/Library/Audio/Plug-Ins/Components/AnalyzerPro.component | 1.1.1 | 1.1.1 | x86_64 arm64 | 2026-06-01 00:06:25 | 2026-06-01 00:12:49 |
| Standalone | ~/Applications/AnalyzerPro.app | 1.1.1 | 1.1.1 | x86_64 arm64 | 2026-06-01 00:06:25 | 2026-06-01 00:14:04 |
| AAX | /Library/Application Support/Avid/Audio/Plug-Ins/AnalyzerPro.aaxplugin | 1.1.1 | 1.1.1 | x86_64 arm64 | 2026-06-01 00:22:53 | 2026-06-01 00:22:52 |

AAX SIGNING / VERIFY
- Signed artifact: build-release/AnalyzerPro_artefacts/Release/AAX/AnalyzerPro.aaxplugin
- Installed signed copy: /Library/Application Support/Avid/Audio/Plug-Ins/AnalyzerPro.aaxplugin
- wraptool sign + verify: PASS
- Installed wraptool verify: PASS
- Signer name: MelechDSP
- Signer GUID: B130BFB6-7A58-F76F-B602-4EAB4DA73A2E
- Product name: AnalyzerPro
- Date signed local: 2026-06-01T00:22:51
- codesign authority: Developer ID Application: AVISHAY LIDANI (C5UC779LGC)
- codesign chain: Developer ID Certification Authority, Apple Root CA
- Signature is Developer ID, not adhoc.

DELETED COPIES
- Removed: ~/Library/Audio/Plug-Ins/VST3/AnalyzerPro.vst3
- Removed: /Library/Audio/Plug-Ins/VST3/AnalyzerPro.vst3
- Removed: ~/Library/Audio/Plug-Ins/Components/AnalyzerPro.component
- Confirmed missing: /Library/Audio/Plug-Ins/Components/AnalyzerPro.component
- Removed: ~/Applications/AnalyzerPro.app
- Confirmed missing: /Applications/AnalyzerPro.app
- Removed: /Library/Application Support/Avid/Audio/Plug-Ins/AnalyzerPro.aaxplugin
- Confirmed missing: ~/Library/Application Support/Avid/Audio/Plug-Ins/AnalyzerPro.aaxplugin

INSTALLED CANONICAL COPIES
- VST3: ~/Library/Audio/Plug-Ins/VST3/AnalyzerPro.vst3
- AU: ~/Library/Audio/Plug-Ins/Components/AnalyzerPro.component
- Standalone: ~/Applications/AnalyzerPro.app
- AAX: /Library/Application Support/Avid/Audio/Plug-Ins/AnalyzerPro.aaxplugin

FINAL CONFIRMATION
- Exactly one common host-scan copy per format:
  - VST3: user VST3 exists; system VST3 missing.
  - AU: user AU exists; system AU missing.
  - Standalone: ~/Applications copy exists; /Applications copy missing.
  - AAX: system Avid copy exists; user Avid copy missing.
- No dev/stale duplicates found in the checked host-scan locations.
- No diagnostics HUD present:
  - Standalone launch/quit spot-check completed.
  - Installed Standalone binary string audit found none of the dev HUD diagnostic strings: paint/s=, timer_jitter_avg_ms=, target_fps=, throttle=, rejected=.

STOP.
