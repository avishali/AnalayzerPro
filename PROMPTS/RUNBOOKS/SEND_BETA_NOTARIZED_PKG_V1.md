MISSION_ID: SEND_BETA_NOTARIZED_PKG_V1

TITLE
Produce a signed + notarized + stapled macOS .pkg of AnalyzerPro and distribute it to beta testers. Works on all macOS versions (10.13+), Intel + Apple Silicon, no Gatekeeper workaround for testers.

WHY THIS METHOD
- Binaries are universal (arm64+x86_64), deployment target 10.13 → they RUN everywhere.
- Notarized + stapled → they OPEN without Gatekeeper warnings on macOS 15 (Sequoia) and older. A .pkg with postinstall places each format in the correct folder (testers don't drag plugins).
- This supersedes the old "simple installer" (which was an UNSIGNED workaround for the macOS-15 pkg-block — a notarized pkg has no such problem).

PREREQUISITES (verified 2026-06-01)
- Developer ID Application: AVISHAY LIDANI (C5UC779LGC) — present.
- Developer ID Installer: AVISHAY LIDANI (C5UC779LGC) — present.
- notarytool keychain profile "AC_PASSWORD" — set up (Apple ID avishay.lidani@gmail.com, team C5UC779LGC).
  If ever missing, recreate once:
    xcrun notarytool store-credentials AC_PASSWORD --apple-id avishay.lidani@gmail.com --team-id C5UC779LGC --password <app-specific-password>
    (app-specific password from appleid.apple.com → Sign-In & Security → App-Specific Passwords)
- build-release/ contains current Release artifacts incl. the PACE-signed AAX (from RESTORE_SHIPPING_STATE). If not, build first: RESTORE_SHIPPING_STATE STEP 1 + sign AAX.
- Version = 1.1.1 (CMake source of truth, via scripts/plugin_version.sh).

HARD RULES
- Use SIGN_AND_NOTARIZE_SKIP_AAX=1 so Apple does NOT re-sign the PACE-wrapped AAX (re-signing breaks PACE).
- Do not change code. Sign/package/notarize only.
- Distribute ONLY the notarized + stapled artifact (the *-signed.pkg). Do not send adhoc/dev builds.

============================================================
IMPLEMENTER
============================================================
STEP 1 — Preflight
- xcrun notarytool history --keychain-profile AC_PASSWORD   (confirms creds work; if it errors, STOP — recreate per PREREQUISITES)
- Confirm build-release/AnalyzerPro_artefacts/Release/ has AU, VST3, AAX, Standalone; AAX codesign authority = Developer ID (PACE), not adhoc.
STOP and report preflight.

STEP 2 — Sign + package + notarize + staple
    SIGN_AND_NOTARIZE_SKIP_AAX=1 ./scripts/sign_and_notarize.sh
This: Developer-ID-signs VST3/AU/Standalone (hardened runtime), keeps PACE AAX, builds the pkg (create_installer.sh), productsigns it (Developer ID Installer), submits to notarytool --wait, staples.
STOP and report: notarytool result (must be "Accepted"), any "Invalid" log URL if it fails, and the signed pkg path.

STEP 3 — Verify the deliverable (must all pass before sending)
    SIGNED=installer/AnalyzerPro-1.1.1-macOS-signed.pkg
    xcrun stapler validate "$SIGNED"                         # "The validate action worked"
    spctl --assess --type install -vv "$SIGNED"              # source=Notarized Developer ID, accepted
    pkgutil --check-signature "$SIGNED"                      # Developer ID Installer chain
- Spot-check the AAX inside the pkg payload remains PACE/Developer-ID signed (not adhoc, not Apple-re-signed).
STOP and report all three results.

STEP 4 — Distribute
- Send installer/AnalyzerPro-1.1.1-macOS-signed.pkg via any channel (Drive/WeTransfer/USB) — notarization travels with the file (stapled), so quarantine on download is fine.
- Tester instructions (include in the send):
  "Double-click AnalyzerPro-1.1.1-macOS-signed.pkg and follow the installer. Installs VST3, AU, AAX, and the Standalone app. Works on macOS 10.13+ (Intel & Apple Silicon). If your DAW was open, rescan plugins / restart it."
STOP and write PROMPTS/MISSIONS/SEND_BETA_RESULT.md (notarization id, staple/spctl/pkgutil results, pkg path, sha256). End with STOP.

============================================================
VERIFIER
============================================================
CHECK 1 — Notarization: notarytool "Accepted"; stapler validate passes; spctl --assess type install = accepted (Notarized Developer ID).
CHECK 2 — pkg signed by Developer ID Installer (pkgutil --check-signature).
CHECK 3 — AAX in payload still PACE/Developer-ID signed (not adhoc, not Apple-re-signed) — SKIP_AAX honored.
CHECK 4 — Version 1.1.1, all four formats present in the pkg, universal arch.
CHECK 5 — No dev HUD / PLUGIN_DEV_MODE in the shipped binaries.
OUTPUT: PROMPTS/MISSIONS/SEND_BETA_VERIFIER.md table. End with STOP.
