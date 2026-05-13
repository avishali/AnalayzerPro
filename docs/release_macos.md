# macOS release: build → PACE AAX → sign → installer → notarize

This document is the **canonical order** for shipping AnalyzerPro on macOS: universal Release build, PACE signing for AAX (when you ship Pro Tools), Apple signing and notarization for everything else, then distribution artefacts.

## What each step does

| Step | Script | Purpose |
|------|--------|--------|
| 1 | `scripts/build_release.sh` | CMake Release + universal binary; writes artefacts under `build-release/AnalyzerPro_artefacts/Release/` (or `ANALYZERPRO_RELEASE_BUILD_DIR`). |
| 2 | `scripts/wraptool_sign_aax.sh` | **Only if you ship AAX.** PACE/Avid `wraptool sign` + `verify` **in place** on the `.aaxplugin` inside the artefacts tree. Wraptool also applies the Apple Developer ID layer PACE expects; you must not run a separate Apple `codesign` over AAX after this for release. |
| 3 | `scripts/sign_and_notarize.sh` | Apple `codesign` on AU, VST3, Standalone; optionally **skips** AAX when `SIGN_AND_NOTARIZE_SKIP_AAX=1`; runs `create_installer.sh` to build `.pkg`/`.zip`/`.dmg` from **current** artefacts; `productsign` on the pkg; `notarytool` + `stapler`; builds **signed** zip/dmg from the artefacts folder. |

**Why AAX is special:** After PACE signing, re-signing the AAX Mach-O with Apple alone can break the intended Pro Tools distribution signature. Use `SIGN_AND_NOTARIZE_SKIP_AAX=1` once AAX has been through `wraptool_sign_aax.sh`.

## One-command release (recommended)

From the repo root, with `JUCE_PATH` set (or in `scripts/.env`) and PACE env configured (see below):

```bash
# Either export JUCE_PATH, or copy scripts/.env.example → scripts/.env and set JUCE_PATH there.
./scripts/release_macos.sh
```

The script runs **build → wraptool (if AAX exists) → sign + notarize** with the correct `SKIP_AAX` flag when AAX was signed.

## Manual sequence (same result)

Run from the **AnalyzerPro** repository root.

### 0. Prerequisites

- **Xcode / CLT**, CMake, **`JUCE_PATH`** (shell export or **`scripts/.env`** — loaded by `source_repo_env.sh` from `build.sh`, `build_release.sh`, `release_macos.sh`).
- **Apple:** Developer ID Application + Developer ID Installer certificates; App Store Connect API key or app-specific password stored for `notarytool` (see `sign_and_notarize.sh` → `NOTARYTOOL_KEYCHAIN_PROFILE`).
- **AAX:** `scripts/.aax_wraptool.env` from `.aax_wraptool.env.example` (gitignored), or the same variables in CI secrets. Details: [aax_pace.md](aax_pace.md).

Optional: same build directory everywhere:

```bash
export ANALYZERPRO_RELEASE_BUILD_DIR="${ANALYZERPRO_RELEASE_BUILD_DIR:-build-release}"
```

### 1. Release build

```bash
./scripts/build_release.sh
```

Artefacts (example paths):

- `build-release/AnalyzerPro_artefacts/Release/AU/AnalyzerPro.component`
- `…/VST3/AnalyzerPro.vst3`
- `…/Standalone/AnalyzerPro.app`
- `…/AAX/AnalyzerPro.aaxplugin` (if the AAX target is enabled)

### 2. PACE-sign AAX (if `AAX/` exists)

Mach-O path (adjust `BUILD_DIR` if you override it):

```bash
./scripts/wraptool_sign_aax.sh \
  "build-release/AnalyzerPro_artefacts/Release/AAX/AnalyzerPro.aaxplugin/Contents/MacOS/AnalyzerPro"
```

Confirm:

```bash
/Applications/PACEAntiPiracy/Eden/Fusion/Current/bin/wraptool verify --verbose --in \
  "build-release/AnalyzerPro_artefacts/Release/AAX/AnalyzerPro.aaxplugin"
```

### 3. Sign plugins, rebuild installer, notarize, archives

If you **PACE-signed** AAX:

```bash
SIGN_AND_NOTARIZE_SKIP_AAX=1 ./scripts/sign_and_notarize.sh
```

If you **do not** ship AAX (no `AAX` folder), you can run:

```bash
./scripts/sign_and_notarize.sh
```

`sign_and_notarize.sh` **always** calls `create_installer.sh` after signing AU/VST3/Standalone (and AAX unless skipped), so the `.pkg` matches the signed binaries in the artefacts tree—including AAX when present.

`create_installer.sh` **defaults to a system-domain installer when the build includes AAX** (so AAX installs to **`/Library/Application Support/Avid/Audio/Plug-Ins`**, the usual machine-wide Avid path). AU/VST3/Standalone then install to **`/Library/...`** and **`/Applications`** as well (admin password). To keep everything under your home folder instead, run with **`INSTALL_DOMAIN=user`** (AAX will then live under **`~/Library/Application Support/Avid/...`**). Set **`ANALYZERPRO_AAX_USE_SYSTEM_WIDE_LIBRARY=0`** when **`INSTALL_DOMAIN` is unset** to default user-only even if AAX is present.

The `.pkg` is a **multi-package** installer: on the Installation Type screen choose **Customize** to enable or disable AU, VST3, Standalone, and AAX (all are selected by default). The installer uses **`pkgbuild --install-location /`** with Apple’s current-user-home or local-system domain so files land under the correct root (not under a literal `$HOME` folder).

### 4. Ship these files

Under `installer/` (exact names depend on `PLUGIN_VERSION` in `sign_and_notarize.sh` / `create_installer.sh`):

- `AnalyzerPro-<version>-macOS-signed.pkg` — stapled, notarized installer.
- `AnalyzerPro-<version>-macOS-signed.zip` — signed format folders from artefacts.
- `AnalyzerPro-<version>-macOS-signed.dmg` — same as zip, disk image.

Keep **plugin version strings** in sync across `build_release.sh`, `create_installer.sh`, and `sign_and_notarize.sh` when you bump releases.

## Optional: installer only (unsigned)

For quick packaging **without** Apple signing or notarization:

```bash
./scripts/create_installer.sh
```

Use `INSTALL_DOMAIN=user` or `INSTALL_DOMAIN=system` to force a domain. If **`INSTALL_DOMAIN` is unset** and the build **includes AAX**, the script defaults to **system** so AAX installs under **`/Library/Application Support/Avid/Audio/Plug-Ins`**. Use **`ANALYZERPRO_AAX_USE_SYSTEM_WIDE_LIBRARY=0`** to keep the default **user** domain even when AAX is present. This **does not** replace the full release path above; for customers you still need `sign_and_notarize.sh` (or equivalent).

## Verification cheatsheet

| Check | Command / action |
|-------|-------------------|
| PACE / AAX | `wraptool verify --verbose --in /path/to/AnalyzerPro.aaxplugin` |
| Apple (Mach-O) | `codesign -dv --verbose=4 …/AnalyzerPro.aaxplugin/Contents/MacOS/AnalyzerPro` |
| Apple (bundle) | `codesign --verify --deep --strict /path/to/bundle` |
| Installer | `pkgutil --check-signature` on the signed pkg; `stapler validate` after staple |

## Troubleshooting pointers

- **Notarization Invalid:** Unsigned payloads, wrong `BUILD_DIR` vs installer, or mismatched paths—see comments in `sign_and_notarize.sh` and `create_installer.sh` (`ANALYZERPRO_RELEASE_BUILD_DIR`).
- **AAX / PACE:** [aax_pace.md](aax_pace.md) (`CouldNotFindSignerCredentials`, iLok vs cloud, `--allowsigningservice`).
- **SKIP_AAX:** Set to `1` only after a successful `wraptool_sign_aax.sh` when including AAX in the same pkg/zip/dmg.
