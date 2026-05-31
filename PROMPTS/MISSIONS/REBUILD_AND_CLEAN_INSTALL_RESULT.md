# REBUILD_AND_CLEAN_INSTALL_V1_1_1 Result

## Build

- Build command: `JUCE_PATH=/Users/avishaylidani/DEV/SDK/JUCE AAX_SDK_PATH=/Users/avishaylidani/Downloads/aax-sdk-2-8-0 ./scripts/build_release.sh`
- Build exit code: `0`
- Confirmed log lines:
  - `Version: 1.1.1`
  - `AnalyzerPro: using AAX SDK from: /Users/avishaylidani/Downloads/aax-sdk-2-8-0`
- Accepted submodule state before build: `third_party/melechdsp-hq` dirty at pinned HEAD `f272a92`
- Accepted submodule diffstat: `26 files changed, 661 insertions(+), 168 deletions(-)`

## Built Artefacts

| Format | Path | CFBundleShortVersionString | CFBundleVersion | Architecture |
| --- | --- | --- | --- | --- |
| VST3 | `build-release/AnalyzerPro_artefacts/Release/VST3/AnalyzerPro.vst3` | `1.1.1` | `1.1.1` | `x86_64 arm64` |
| Standalone | `build-release/AnalyzerPro_artefacts/Release/Standalone/AnalyzerPro.app` | `1.1.1` | `1.1.1` | `x86_64 arm64` |
| AAX | `build-release/AnalyzerPro_artefacts/Release/AAX/AnalyzerPro.aaxplugin` | `1.1.1` | `1.1.1` | `x86_64 arm64` |

## AAX Signing

- Signing command: `./scripts/wraptool_sign_aax.sh build-release/AnalyzerPro_artefacts/Release/AAX/AnalyzerPro.aaxplugin/Contents/MacOS/AnalyzerPro`
- Result: PASS
- Wraptool verify: PASS
- Signer: `MelechDSP`
- Product: `AnalyzerPro`
- Date Signed local: `2026-05-31T22:14:46`

## Deleted Copies

| Path | Result |
| --- | --- |
| `/Library/Audio/Plug-Ins/VST3/AnalyzerPro.vst3` | Already absent |
| `~/Library/Audio/Plug-Ins/VST3/AnalyzerPro.vst3` | Deleted |
| `/Applications/AnalyzerPro.app` | Already absent |
| `~/Applications/AnalyzerPro.app` | Deleted |
| `/Library/Application Support/Avid/Audio/Plug-Ins/AnalyzerPro.aaxplugin` | Deleted via administrator prompt |

## Installed Copies

| Format | Path | Version | MTime | Architecture |
| --- | --- | --- | --- | --- |
| VST3 | `~/Library/Audio/Plug-Ins/VST3/AnalyzerPro.vst3` | `1.1.1` | `2026-05-31 21:53:36 +0300` | `x86_64 arm64` |
| Standalone | `~/Applications/AnalyzerPro.app` | `1.1.1` | `2026-05-31 21:53:36 +0300` | `x86_64 arm64` |
| AAX | `/Library/Application Support/Avid/Audio/Plug-Ins/AnalyzerPro.aaxplugin` | `1.1.1` | `2026-05-31 22:14:49 +0300` | `x86_64 arm64` |

## Final Confirmation

- Exactly one `AnalyzerPro.vst3` remains across `/Library` and `~/Library`: PASS (`~/Library` only)
- Exactly one `AnalyzerPro.app` remains across `/Applications` and `~/Applications`: PASS (`~/Applications` only)
- Installed system AAX is signed and verifies with wraptool: PASS
- AU/component installs were not deleted or replaced: PASS

STOP
