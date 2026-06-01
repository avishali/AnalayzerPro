# SEND_BETA_RESULT — AnalyzerPro 1.1.1 beta

Date: 2026-06-01

## Deliverable (ready to send)
- File: installer/AnalyzerPro-1.1.1-macOS-signed.pkg
- Size: ~16 MB
- SHA-256: 7ec0279552dc8b3cbb28a495b650945e3be9b769d4376eed782b4602bad63a42

## Verification (all passed)
- notarytool: Accepted — submission ID 61910938-2d67-4c2b-aa89-f7799b06dd0a
- stapler validate: worked (stapled → offline Gatekeeper accepts)
- spctl --assess --type install: accepted, source=Notarized Developer ID, origin=Developer ID Installer: AVISHAY LIDANI (C5UC779LGC)
- pkgutil --check-signature: Developer ID Installer chain verified, notarization trusted
- Packaged AAX spot-check: Developer ID Application: AVISHAY LIDANI (C5UC779LGC), universal x86_64+arm64, __Pace_Eden.bundle validated (PACE intact, not Apple-re-signed — SKIP_AAX honored)

## Build provenance
- Version 1.1.1 (CMake source of truth). Universal arm64+x86_64, deployment target 10.13.
- PLUGIN_DEV_MODE=OFF (no dev HUD). Formats: AU, VST3, AAX, Standalone.
- Built from pinned submodule melechdsp-hq @ b2983a9 (branch analyzerpro/glassy-motion-and-smoothing); parent @ 9ceb26a.
- Includes Phase 2 glassy-motion render + cascaded-box triangular spectral smoothing.

## Tester note
"AnalyzerPro 1.1.1 beta — double-click the .pkg and follow the installer. Installs VST3, AU, AAX, and the Standalone app. macOS 10.13+ (Intel & Apple Silicon). If your DAW was open, rescan plugins or restart it."

## Known/open
- Pre-existing Side-trace toggle ("scrolling menu") UI bug — tracked separately, not in this build's scope.
- AAX is signed with the developer's own Developer ID + PACE; loads on machines that trust it. Avid-store/registration-specific AAX signing is a separate concern.

STOP.
