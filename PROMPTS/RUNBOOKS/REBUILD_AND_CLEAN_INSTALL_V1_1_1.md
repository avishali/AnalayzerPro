MISSION_ID: REBUILD_AND_CLEAN_INSTALL_V1_1_1

TITLE
Rebuild AnalyzerPro 1.1.1 (Release) and replace all stale/duplicate installs with one canonical copy per format, so display A/B testing compares the correct build.

GOAL
Guarantee that whatever a DAW or Pro Tools loads is the freshly built 1.1.1 binary that includes the current working-tree code. Eliminate the "two builds, same version string" ambiguity found on 2026-05-31 (duplicate installs in /Library and ~/Library with identical 1.1.1 version but different build dates).

CONTEXT (already done — do NOT redo)
- Version drift in packaging scripts is FIXED. All scripts now derive PLUGIN_VERSION from CMakeLists via scripts/plugin_version.sh (verified prints "Version: 1.1.1"). Do NOT re-add hardcoded version literals.
- Source of truth: CMakeLists.txt PLUGIN_VERSION_MAJOR/MINOR/PATCH = 1.1.1.
- Build env (NOT in scripts/.env, which still has the /path/to/JUCE placeholder — pass inline):
    JUCE_PATH=/Users/avishaylidani/DEV/SDK/JUCE
    AAX_SDK_PATH=/Users/avishaylidani/Downloads/aax-sdk-2-8-0
- AAX PACE/wraptool signing credentials live in scripts/.aax_wraptool.env and scripts/wraptool_sign_aax.sh (per repo owner, all info needed is present locally).
- Machine is arm64.

HARD RULES
- No source/DSP/UI code changes in this mission. Build + sign + install + verify ONLY.
- Build configuration MUST be Release (perf-honest). Universal binary (arm64 + x86_64) as produced by scripts/build_release.sh.
- Do NOT touch the AU (.component) installs — AU is out of scope this round; leave existing AU copies in place.
- Deleting from /Library and /Applications requires sudo — prompt the operator; do not assume passwordless sudo.
- AAX has NO user-level scan path: it MUST be installed to the system Avid path /Library/Application Support/Avid/Audio/Plug-Ins. The "~/Library only" rule applies to VST3 and Standalone only.

FILES / LOCATIONS
- Build output: build-release/AnalyzerPro_artefacts/Release/{VST3,Standalone,AAX}/
- Install targets:
    VST3       → ~/Library/Audio/Plug-Ins/VST3/AnalyzerPro.vst3
    Standalone → ~/Applications/AnalyzerPro.app
    AAX        → /Library/Application Support/Avid/Audio/Plug-Ins/AnalyzerPro.aaxplugin
- Stale/duplicate copies to remove (confirmed present 2026-05-31):
    /Library/Audio/Plug-Ins/VST3/AnalyzerPro.vst3                       (sudo)
    ~/Library/Audio/Plug-Ins/VST3/AnalyzerPro.vst3
    /Applications/AnalyzerPro.app                                        (sudo)
    ~/Applications/AnalyzerPro.app
    /Library/Application Support/Avid/Audio/Plug-Ins/AnalyzerPro.aaxplugin (sudo, AAX — replaced not just deleted)

============================================================
IMPLEMENTER PROMPT
============================================================

ROLE
You are the build/release operator. You are producing correct binaries and a clean install. No code edits.

STEP 0 — Confirm clean starting state
- git status. The EXPECTED working-tree set (all accounted-for, none are surprises) is:
    Source/ui/analyzer/AnalyzerDisplayView.cpp/.h   (WIP cadence/ballistics + Phase 0.1 diagnostics)
    scripts/{build_release,create_installer,create_simple_installer,sign_and_notarize}.sh + scripts/plugin_version.sh   (version-from-CMake fix)
    PROMPTS/RUNBOOKS/*.md + PROMPTS/INDEX/RUNBOOK_INDEX.md   (process docs)
    third_party/melechdsp-hq                         (submodule — SEE CRITICAL NOTE BELOW)
- `m third_party/melechdsp-hq`: the submodule has ~660 lines of UNCOMMITTED edits across the rendering library (RTADisplay, SnapshotPump, AnalyzerDisplayWidget, mdsp_ui). OWNER DECISION 2026-05-31: this is intentional in-flight WIP and is the accepted build baseline — do NOT block on it, do NOT commit/revert it. Caveat (acknowledged): builds are not bit-reproducible while the submodule is dirty. Record the submodule HEAD (must equal parent pin f272a92) and a `git -C third_party/melechdsp-hq diff --stat` snapshot in the result file for the record, then proceed.
- Confirm CMakeLists version = 1.1.1.
STOP and report working-tree state + version + submodule status.

STEP 1 — Release universal build (all formats)
Run:
    JUCE_PATH=/Users/avishaylidani/DEV/SDK/JUCE \
    AAX_SDK_PATH=/Users/avishaylidani/Downloads/aax-sdk-2-8-0 \
    ./scripts/build_release.sh
Expected log lines: "Version: 1.1.1", "using AAX SDK from: .../aax-sdk-2-8-0".
STOP and report: build exit code, and that VST3 + Standalone + AAX artefacts exist under build-release/AnalyzerPro_artefacts/Release/.

STEP 2 — Verify freshly built versions + architectures
For VST3, Standalone, AAX built artefacts, print:
- CFBundleShortVersionString and CFBundleVersion (expect 1.1.1)
- lipo -info on the binary (expect arm64 + x86_64)
STOP and report a small table.

STEP 3 — Sign the AAX (PACE/wraptool)
Use the repo signing pipeline:
    source scripts/.aax_wraptool.env   (or as wraptool_sign_aax.sh expects)
    ./scripts/wraptool_sign_aax.sh     (sign the build-release AAX)
Verify the resulting .aaxplugin is signed (wraptool verify / no unsigned warning).
STOP and report signing result. If wraptool is not found or credentials fail, STOP and report the exact error — do NOT proceed to install an unsigned AAX.

STEP 4 — Remove stale/duplicate installs
Delete the duplicate VST3 and Standalone copies listed under FILES/LOCATIONS (both /Library + ~ paths; /Library needs sudo). For AAX, the system Avid copy will be overwritten in STEP 5, so removing it first is optional.
Do NOT touch any .component (AU) copies.
STOP and report what was deleted (with sudo prompts surfaced to the operator).

STEP 5 — Install fresh, one canonical copy per format
- VST3       → ~/Library/Audio/Plug-Ins/VST3/AnalyzerPro.vst3
- Standalone → ~/Applications/AnalyzerPro.app
- AAX (signed) → /Library/Application Support/Avid/Audio/Plug-Ins/AnalyzerPro.aaxplugin (sudo)
Prefer using scripts/create_installer.sh / sign_and_notarize.sh if that is the normal path; otherwise copy artefacts directly. State which method you used.
STOP and report installed paths.

STEP 6 — Post-install verification (the whole point of this mission)
For each installed copy, print version + build date (stat mtime) and confirm:
- Exactly ONE AnalyzerPro.vst3 across /Library and ~/Library (the new one).
- Exactly ONE AnalyzerPro.app across /Applications and ~/Applications (the new one).
- The system AAX is the new signed build.
- All report 1.1.1 AND all share the same (today's) build date.
STOP and write PROMPTS/MISSIONS/REBUILD_AND_CLEAN_INSTALL_RESULT.md with:
- build exit code
- version+arch table (built artefacts)
- AAX signing result
- deleted copies
- installed copies (path + version + mtime)
- confirmation: no duplicate VST3/Standalone remain
End with STOP.

============================================================
VERIFIER PROMPT
============================================================

ROLE
Verify the machine now has exactly one correct 1.1.1 build per in-scope format.

CHECK 1 — Build correctness
PASS if built VST3/Standalone/AAX report 1.1.1 and are universal (arm64 + x86_64).

CHECK 2 — AAX signed
PASS if the installed system AAX passes wraptool verification (not unsigned).

CHECK 3 — No duplicates
PASS if there is exactly one AnalyzerPro.vst3 (in ~/Library) and one AnalyzerPro.app (in ~/Applications) reachable by host scanning; FAIL if a /Library or duplicate copy still exists.

CHECK 4 — Freshness
PASS if every installed in-scope copy has today's build date (not the stale May 12/13/27 dates).

CHECK 5 — AU untouched
PASS if existing .component copies are unchanged (not deleted, not replaced).

OUTPUT
Write PROMPTS/MISSIONS/REBUILD_AND_CLEAN_INSTALL_VERIFIER_RESULT.md:
| Check | Status | Notes |
| Build correctness | PASS/FAIL | |
| AAX signed | PASS/FAIL | |
| No duplicates | PASS/FAIL | |
| Freshness | PASS/FAIL | |
| AU untouched | PASS/FAIL | |
End with STOP.
