# Diagnostics & Crash Reporting System - Invariants

## Purpose

The RTA1 diagnostics and crash-reporting system provides DEBUG-only visibility into plugin lifecycle, runtime state, and crash detection. It uses a fixed-size ring buffer for in-memory event logging and periodic disk snapshots to survive hard crashes. The system is designed to be realtime-safe (no allocations/locks on audio thread) and has zero overhead in Release builds.

## Threading Invariants

**CRITICAL: These must never be violated.**

- **No file I/O on audio thread**: All file operations (checkpoint writes, crash packet reads, cleanup) must run on the message thread only.
- **No allocations/locks in processBlock**: The `Diagnostics::log()` method uses atomic operations only; no heap allocations or mutex locks.
- **APVTS parameterChanged is audio-thread safe**: The `parameterChanged()` callback may run on the audio thread. It must NOT touch UI components directly. Use `AsyncUpdater` to defer UI updates to the message thread.
- **All checkpoint writes occur on message thread**: `CrashReporter::checkpoint()` and `checkpointNow()` must only be called from the message thread (enforced via `jassert` in DEBUG builds).

## Lifecycle Invariants

- **dspReady gate**: The `dspReady` flag must be `false` during `prepareToPlay()`, `releaseResources()`, and any analyzer rebuilds. UI frame pulls (`updateDisplayFromAnalyzer()`) must check `isDspReady()` before accessing analyzer data.
- **Headless pump activation**: The `HeadlessCheckpointPump` timer must only call `performDiagnosticCheckpoint()` when `openEditorCount == 0`. When an editor opens, the pump stops; when the last editor closes, it resumes.
- **Editor count balance**: `debugEditorOpened()` and `debugEditorClosed()` must be called in matched pairs. The `openEditorCount` must never go negative (enforced via `jassert` in DEBUG builds).

## Persistence Invariants

- **Crash flag semantics**: If `RTA1_CRASH_FLAG_<instanceId>` exists at startup, the previous run likely crashed (flag was not cleared on clean exit).
- **Packet file format**: Crash packets are fixed-size binary POD structures with magic number `0x52544131` ("RTA1") and version `1`. Files are validated on read.
- **Per-instance file naming**: Each plugin instance gets a unique `instanceId` (monotonic counter). Files are named:
  - `RTA1_CRASH_FLAG_<instanceId>`
  - `RTA1_CRASH_PACKET_<instanceId>.bin`
  - `RTA1_CRASH_PACKET_<instanceId>_last.bin`
  This prevents collisions when multiple instances run simultaneously.

## Operational Tips

### Dumping Diagnostics

- **Keyboard shortcut**: Press `Cmd+D` in the plugin editor (DEBUG builds only).
- **Diagnostics panel**: Click the "Diag" button in the control bar to show the diagnostics panel, then click "Dump Diagnostics".
- **Programmatic**: Call `processor.dumpDiagnostics()` from message thread.

### File Locations (macOS)

Crash files are stored in:
```
~/Library/Application Support/RTA1/
```

Files are automatically cleaned up on startup, keeping only the 10 most recent instance families.

### Testing Crash Detection

To test "previous run crash" detection:
1. Load the plugin in a host (Standalone or VST3).
2. Let it run for a few seconds (to generate checkpoint files).
3. Force-kill the host process (e.g., `kill -9 <pid>` or Activity Monitor).
4. Reload the plugin in the same or different host.
5. Check the console/log for "PREVIOUS RUN CRASH DETECTED" message with diagnostic dump.

## Do Not Do This (Anti-Patterns)

1. **❌ Calling `dumpDiagnostics()` from audio thread**: This method allocates strings and performs file I/O. Use it only from the message thread.

2. **❌ Touching UI in `parameterChanged()`**: This callback may run on the audio thread. Never call `setVisible()`, `repaint()`, or any UI method directly. Use `AsyncUpdater::triggerAsyncUpdate()` instead.

3. **❌ Checking `dspReady` without gating**: Always check `isDspReady()` before pulling analyzer frames in UI code. Accessing analyzer data when `dspReady == false` can cause crashes.

4. **❌ Calling `performDiagnosticCheckpoint()` from audio thread**: Checkpoint writes require file I/O. Use the atomic flag `diagCheckpointRequested` to request checkpoints from audio thread, then handle them on message thread.

5. **❌ Modifying `openEditorCount` without matching calls**: Each `debugEditorOpened()` must have a corresponding `debugEditorClosed()`. Imbalanced calls will break headless checkpoint pump behavior.

6. **❌ Bypassing throttle in `checkpoint()`**: The 2-second throttle prevents excessive disk writes. Use `checkpointNow()` only when explicitly needed (e.g., "Force Checkpoint" button).

7. **❌ Accessing `Diagnostics` members without atomic operations**: The ring buffer uses atomic write index. Direct array access without proper synchronization can cause data races.

8. **❌ Removing `#if JUCE_DEBUG` guards**: All diagnostics code must be wrapped in `#if JUCE_DEBUG` to ensure zero overhead in Release builds. Removing guards will break Release build compilation or add unnecessary code.

## Validation

To verify the system is working correctly:

1. **Crash detection**: Force-kill a plugin instance, then reload. Check console for crash dump.
2. **Headless operation**: Load plugin without opening editor. Verify crash files are created (check `~/Library/Application Support/RTA1/`).
3. **Editor lifecycle**: Open and close editor multiple times. Verify `openEditorCount` stays balanced (check via diagnostics panel or logs).
4. **File cleanup**: Run many plugin sessions. Verify only ~10 most recent instance families remain in crash directory.
5. **Realtime safety**: Monitor audio thread for allocations/locks using profiling tools. Verify no diagnostics-related allocations occur in `processBlock()`.
