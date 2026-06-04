# AnalyzerPro Beta Release — Handoff (for Cursor)

Goal: produce **signed + notarized** AnalyzerPro installers (AU / VST3 / AAX / Standalone,
universal arm64+x86_64) for beta testers — including **PT/AAX** users.

All code is landed and the build is proven to compile. What remains is committing one
script fix, running the signed release pipeline, verifying, and distributing.

## Current state (verified)
- **Canonical HQ**: committed + **pushed** → `melechdsp-hq` `782d2aac` (PAZ phase fan + goniometer L/R fix).
- **AnalyzerPro**: committed **locally** → `83fdfc4` (header IA, trace colors, meter fixes, PAZ phase fan, draggable Release, thin bezel; submodule bumped to `782d2aac`). **Not yet pushed.**
- **Dev HUD** (`[Standalone UI] tick=VBlank…`): already gated by `PLUGIN_DEV_MODE`. A Release
  build (`PLUGIN_DEV_MODE=OFF`) compiles it out — no code change needed. We had only been
  running DEV builds.
- **Uncommitted, already applied**: `scripts/build_release.sh` now passes
  `-DANALYZERPRO_COPY_AFTER_BUILD=OFF` (see Job 1 — must be committed).

## Signing prerequisites — all present on this machine (verified)
- Developer ID **Application** cert: `AVISHAY LIDANI (C5UC779LGC)`
- Developer ID **Installer** cert: `AVISHAY LIDANI (C5UC779LGC)`
- notarytool keychain profile: `AC_PASSWORD` (works)
- PACE **wraptool**: `/Applications/PACEAntiPiracy/Eden/Fusion/Versions/5/bin/wraptool`; iLok Manager installed
- AAX wraptool creds: `scripts/.aax_wraptool.env` (gitignored, present)
- AAX SDK: `/Users/avishaylidani/DEV/SDK/aax-sdk-2-9-0`
- JUCE: `/Users/avishaylidani/DEV/SDK/JUCE`

## Required environment (every release command)
`scripts/.env` has a **placeholder** `JUCE_PATH=/path/to/JUCE`, and `AAX_SDK_PATH` is not in
`.env`. So **export both** before running, or fix `scripts/.env`:
```bash
export JUCE_PATH=/Users/avishaylidani/DEV/SDK/JUCE
export AAX_SDK_PATH=/Users/avishaylidani/DEV/SDK/aax-sdk-2-9-0
```

---

## Job 1 — Commit the release-script fix
```bash
cd /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/AnalyzerPro
git add scripts/build_release.sh
git commit -m "build: release build must not auto-install to system folders (COPY_AFTER_BUILD=OFF)"
```
Why: `ANALYZERPRO_COPY_AFTER_BUILD` defaults ON; a Release build then tries to copy the AAX
bundle into the system `/Library/Application Support/Avid/Audio/Plug-Ins/`, which needs root
and **fails** ("Operation not permitted"). Release builds package artifacts, not auto-install.
(This was the only thing that broke the pipeline; the code compiles cleanly.)

## Job 2 — Run the full signed release pipeline
```bash
export JUCE_PATH=/Users/avishaylidani/DEV/SDK/JUCE
export AAX_SDK_PATH=/Users/avishaylidani/DEV/SDK/aax-sdk-2-9-0
bash scripts/release_macos.sh
```
This does, in order (see `docs/release_macos.md`):
1. `build_release.sh` — clean Release build, universal, all 4 formats, `PLUGIN_DEV_MODE=OFF`.
2. `wraptool_sign_aax.sh` — PACE-sign the AAX (uses iLok; may prompt / need iLok running).
3. `sign_and_notarize.sh` — Apple codesign all bundles → `productsign` `.pkg` → `notarytool`
   submit + **wait** (Apple round-trip, a few min) → `stapler` → zip/dmg archives.

Expect ~10–15 min total. Likely snags to watch: iLok/wraptool auth (Job-2 step 2), and
notarization status (must end `status: Accepted` before stapling).

## Job 3 — Verify the artifacts
```bash
ART=build-release/AnalyzerPro_artefacts/Release
lipo -info "$ART/Standalone/AnalyzerPro.app/Contents/MacOS/AnalyzerPro"   # expect: arm64 x86_64
codesign -dv --verbose=4 "$ART/VST3/AnalyzerPro.vst3" 2>&1 | grep -i authority   # Developer ID
spctl -a -vvv -t install <signed .pkg>     # accepted / notarized
xcrun stapler validate <signed .pkg>       # The validate action worked
```
- **Launch the Release Standalone and confirm the `[Standalone UI] …` dev HUD is GONE.**
- Confirm AAX is PACE-signed (`wraptool verify` per `docs/aax_pace.md`) and loads in Pro Tools.

## Job 4 — Push the product commit
When the artifacts are verified, push AnalyzerPro:
```bash
git -C /Users/avishaylidani/DEV/GitHubRepo/MelechDSP/AnalyzerPro push origin master
```
(`83fdfc4` references the already-pushed HQ `782d2aac`, so it's safe to push.)

## Job 5 — MasterLimiter HQ submodule (decision)
HQ moved `77759aa → 782d2aac`, but `782d2aac` is **analyzer-only** (phase fan + goniometer).
MasterLimiter doesn't need it. **Default: leave MasterLimiter's submodule at `77759aa`.**
Only bump it if you want both products on the same HQ commit (then rebuild/verify MasterLimiter).

## Job 6 — Distribute to beta testers
Ship the notarized `.pkg` (and/or zip/dmg) produced by Job 2. Include AU + VST3 + AAX +
Standalone so PT/AAX testers are covered. Note the build/version comes from CMake
(`scripts/plugin_version.sh` is the source of truth).

## Done criteria
- Signed + notarized + stapled `.pkg` (and archives) for universal AU/VST3/AAX/Standalone.
- Release Standalone shows **no dev HUD**.
- AAX PACE-signed and loads in Pro Tools.
- `scripts/build_release.sh` fix committed; AnalyzerPro `master` pushed.
