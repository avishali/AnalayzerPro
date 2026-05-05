# AnalyzerPro v1.1.1 Release Baseline

This document defines the first production release build matrix and requirements.

## Target Build Matrix

- Standalone:
  - macOS universal (`x86_64` + `arm64`) with minimum deployment target `10.15`
  - Windows `x64`
  - Windows `ARM64`
- VST3:
  - macOS universal
  - Windows `x64` and `ARM64`
- AU:
  - macOS universal only
- AAX:
  - macOS and Windows (see requirements below)

## CMake Presets

Use these presets for release builds:

- `release-macos-universal`
- `release-windows-x64`
- `release-windows-arm64`

Examples:

```bash
cmake --preset release-macos-universal
cmake --build --preset release-macos-universal
```

```bash
cmake --preset release-windows-x64
cmake --build --preset release-windows-x64
```

## AAX Requirements (What we still need)

AAX distribution has extra requirements beyond building:

- Avid AAX SDK installed and available to the build toolchain.
- Pro Tools validation on supported versions/OSes.
- Proper signing for AAX bundles:
  - macOS: Developer ID signing for distribution builds.
  - Windows: Authenticode signing certificate.
- If releasing publicly, complete Avid partner/distribution requirements.
- Verify plugin metadata and IDs are stable (manufacturer/plugin code/versioning).

Without signing + validation, AAX binaries may build but fail to load in production Pro Tools environments.

## Recommended Release Flow

1. Build all release presets.
2. Smoke test Standalone, VST3, AU, AAX on target hosts.
3. Sign binaries.
4. Package installers/artifacts per platform.
5. Tag release (`v1.1.1`) and publish notes.
