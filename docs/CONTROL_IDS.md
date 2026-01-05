# CONTROL_IDS — AnalyzerPro (v0)

## Purpose
`CONTROL_IDS` defines a stable, hardware-ready contract for all user-facing controls in AnalyzerPro.

Rules:
- UI widgets bind to `CONTROL_IDS`, not directly to parameters or hardware.
- Hardware (future) maps to `CONTROL_IDS`, not to UI implementation details.
- DSP parameters may change implementation, but `CONTROL_IDS` should remain stable once frozen.

Lifecycle: **DRAFT → REVIEW → FROZEN → DEPRECATED**  
See `docs/diagrams/CONTROL_IDS_Lifecycle_Diagram.svg`.

---

## Status & Versioning

- **Document version:** v0
- **Current lifecycle stage:** DRAFT
- **Breaking changes allowed:** Yes (while DRAFT)
- **Freeze criteria:** See “Freeze Policy” below

When this reaches **FROZEN**, treat it as a hard contract for:
- Hardware controllers
- Firmware mappings
- MIDI/HID/OSC protocol adapters

---

## Naming & Format Rules

### Control ID format
All IDs use: