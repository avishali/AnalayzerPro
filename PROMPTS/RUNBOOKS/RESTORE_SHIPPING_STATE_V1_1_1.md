MISSION_ID: RESTORE_SHIPPING_STATE_V1_1_1

TITLE
End-of-investigation cleanup: rebuild the SHIPPING (clean, no-HUD) 1.1.1, re-sign AAX, and reinstall one canonical signed copy per format (incl. AU) — undoing the dev/HUD/adhoc state left by the smoothness investigation.

WHEN TO RUN
After the Phase 2 (glassy motion) work is done and before any beta hand-off. The dev iterations left the machine in a non-shippable state (see CONTEXT).

CONTEXT (state left by the investigation, 2026-05-31)
- The canonical installs were overwritten by dev builds (PLUGIN_DEV_MODE=1, HUD visible) because COPY_PLUGIN_AFTER_BUILD auto-installs every build.
- The system AAX was overwritten with an adhoc/UNSIGNED build (codesign Signature=adhoc) — Pro Tools won't load it.
- The surviving AU (.component) is a STALE May 27 build (system /Library AU was removed during the rebuild mission; only ~/Library AU remains, old).
- Version source of truth = CMakeLists 1.1.1; scripts derive it via scripts/plugin_version.sh.
- Build env: JUCE_PATH=/Users/avishaylidani/DEV/SDK/JUCE, AAX_SDK_PATH=/Users/avishaylidani/Downloads/aax-sdk-2-8-0. iLok must be connected for AAX signing.
- If Phase 2 added option ANALYZERPRO_COPY_AFTER_BUILD (default ON), the shipping build must NOT set it OFF (we WANT the install copy here) — or install explicitly.

HARD RULES
- SHIPPING build = Release, PLUGIN_DEV_MODE=OFF, universal (arm64+x86_64). NO HUD in the result.
- All FOUR formats rebuilt from the same tree: VST3, AU, Standalone, AAX.
- AAX MUST be PACE-signed and verify (no adhoc). Do NOT install an unsigned AAX.
- One canonical copy per format; remove dev/stale duplicates.
- Do not change source code. Build/sign/install/verify only.

FILES / LOCATIONS
- Shipping build dir: build-release/ (NOT build-release-dev).
- Installs: VST3 → ~/Library/Audio/Plug-Ins/VST3 ; AU → ~/Library/Audio/Plug-Ins/Components ; Standalone → ~/Applications ; AAX → /Library/Application Support/Avid/Audio/Plug-Ins (system, sudo).

============================================================
IMPLEMENTER PROMPT
============================================================

ROLE
Build/release operator. Restore a clean, signed, shippable 1.1.1. No code edits.

STEP 1 — Clean shipping build (no HUD, all formats)
    JUCE_PATH=/Users/avishaylidani/DEV/SDK/JUCE \
    AAX_SDK_PATH=/Users/avishaylidani/Downloads/aax-sdk-2-8-0 \
    ./scripts/build_release.sh
(build_release.sh = Release, -DPLUGIN_DEV_MODE=OFF, universal, build-release/. Confirm FORMATS includes AU; if not, add it for this build.)
Expected: "Version: 1.1.1", "using AAX SDK from: .../aax-sdk-2-8-0".
STOP and report: exit code; VST3/AU/Standalone/AAX artefacts present under build-release/AnalyzerPro_artefacts/Release/.

STEP 2 — Verify the build is SHIPPING, not dev
- All four report CFBundleShortVersionString=CFBundleVersion=1.1.1 and lipo shows x86_64 arm64.
- HUD ABSENT: confirm PLUGIN_DEV_MODE=0 in build-release CMakeCache (Dev Mode: 0), i.e. ANALYZERPRO_DEV_DIAGNOSTICS off — no diagnostics overlay in the shipping binary.
STOP and report the table + Dev Mode=0 confirmation.

STEP 3 — PACE-sign AAX (iLok connected)
    ./scripts/wraptool_sign_aax.sh \
      build-release/AnalyzerPro_artefacts/Release/AAX/AnalyzerPro.aaxplugin/Contents/MacOS/AnalyzerPro
Then verify: wraptool verify AND codesign -dv must show a real Developer ID authority (NOT Signature=adhoc).
STOP and report signing + verify result. If it fails, STOP (do not install unsigned AAX).

STEP 4 — Remove dev/stale installed copies
Delete any existing installed copies for all four formats (dev/HUD VST3 at ~/Library, adhoc AAX at the Avid system path [sudo], stale AU, dev Standalone). /Library + system Avid need sudo.
STOP and report deletions.

STEP 5 — Install one canonical signed copy per format
- VST3 → ~/Library/Audio/Plug-Ins/VST3/AnalyzerPro.vst3
- AU → ~/Library/Audio/Plug-Ins/Components/AnalyzerPro.component
- Standalone → ~/Applications/AnalyzerPro.app
- AAX (signed) → /Library/Application Support/Avid/Audio/Plug-Ins/AnalyzerPro.aaxplugin (sudo)
STOP and report installed paths.

STEP 6 — Final shippable verification
For each installed copy report version + arch + build date, and:
- Exactly ONE copy per format reachable by host scanning (no dev/stale duplicates).
- AAX: wraptool verify PASS and codesign authority = Developer ID (NOT adhoc).
- No diagnostics HUD present (spot-check: launch Standalone — no overlay line).
STOP and write PROMPTS/MISSIONS/RESTORE_SHIPPING_STATE_RESULT.md:
- build exit + Dev Mode=0
- version/arch table (all 4)
- AAX signing + verify (authority shown)
- deleted + installed copies
- confirmation: no dev/stale duplicates, no HUD
End with STOP.

============================================================
VERIFIER PROMPT
============================================================

ROLE
Verify the machine is in a clean, signed, shippable state.

CHECK 1 — Shipping build: all 4 formats 1.1.1, universal, Dev Mode=0 (no HUD).
CHECK 2 — AAX signed: wraptool verify PASS; codesign authority = Developer ID (NOT adhoc).
CHECK 3 — Canonical single copy per format; no dev/stale duplicates remain.
CHECK 4 — No diagnostics overlay in any installed binary.
CHECK 5 — Versions all 1.1.1 with the same (today's) build date.

OUTPUT
Write PROMPTS/MISSIONS/RESTORE_SHIPPING_STATE_VERIFIER_RESULT.md:
| Check | Status | Notes |
| Shipping build (Dev Mode=0) | PASS/FAIL | |
| AAX signed (not adhoc) | PASS/FAIL | |
| No duplicates | PASS/FAIL | |
| No HUD | PASS/FAIL | |
| Versions/date | PASS/FAIL | |
End with STOP.
