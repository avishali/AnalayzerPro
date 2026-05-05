# AAX Signing Approval Request - AnalyzerPro v1.1.1

Use this template to contact Avid/PACE and request production AAX signing approval.

## Subject

AAX signing approval request - AnalyzerPro v1.1.1 (MelecDSP)

## Email Body

Hello Avid/PACE team,

I am requesting guidance and approval to complete the AAX production distribution signing flow for our plugin.

Company / Publisher
- Company name: MelecDSP
- Contact name: Avishay Lidani
- Contact email: avishay.lidani@gmail.com
- iLok/PACE account: avishayl
- Avid developer account email: avishay.lidani@gmail.com

Product
- Product name: AnalyzerPro
- Plugin format: AAX
- Release candidate version: 1.1.1
- Manufacturer code: Melc
- Plugin code: AnPr
- AAX bundle identifier: com.MelecDSP.AnalyzerPro
- CFBundleVersion: 1.1.1
- CFBundleShortVersionString: 1.1.1
- Target platforms:
  - macOS 10.15+ (universal: x86_64 + arm64)
  - Windows (x64 + ARM64 release presets prepared)

Current status
- AAX builds successfully and loads in Pro Tools Developer environment.
- QA status for AAX v1.1.1: PASS (see attached QA report).
- Functional checks passed: discovery, instantiate, automation, bypass/audio path, multi-instance stability, plugin preset save/load.
- Note: full Pro Tools session save/reopen could not be validated in Developer mode due to host limitation.

Request
Please provide the required production signing workflow details for our account, including:
1. Account enablement/approval prerequisites for AAX production signing.
2. Required tooling/workflow to sign AnalyzerPro.aaxplugin for commercial distribution.
3. Required metadata/submission package details (identifiers, manifests, certificates, hashes, etc.).
4. Validation requirements before final release sign-off.

If helpful, I can provide immediately:
- Release candidate bundle: AnalyzerPro.aaxplugin
- QA report: docs/QA_AAX_V1_1_1.md
- Release metadata and identifiers listed above
- SHA256 of the release-candidate AAX binary:
  56b33f5c03c0e13fc40692668bba7ab6ac3b6c8a2fafc3605d09b8649c578a02

Thank you,
Avishay Lidani
MelecDSP

## Attachments To Include

- `docs/QA_AAX_V1_1_1.md`
- `AnalyzerPro.aaxplugin` (or requested package format)
- Optional: release notes for v1.1.1

## Notes

- Apple Developer ID signing/notarization is already in place for macOS distribution artifacts.
- Final public/commercial AAX release remains pending PACE/Avid production signing completion.
