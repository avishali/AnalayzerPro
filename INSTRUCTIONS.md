# AnalyzerPro Build Commands

Copy-paste commands for terminal. Run from the AnalyzerPro project root.

---

## Prerequisites (set once per session)

Required before any build. Replace `/path/to/JUCE` with your actual JUCE installation path (e.g. `~/JUCE` or `/opt/JUCE`).

```bash
export JUCE_PATH=/path/to/JUCE
```

---

## Debug Build (fast, for development)

Use for day-to-day development. Includes debug symbols, no optimizations, faster compile. Produces larger binaries.

- **rm -rf build-debug** — Remove old build folder to force a clean configure
- **cmake -B build-debug …** — Configure CMake; Ninja is the generator; Debug build type
- **cmake --build …** — Compile the project

```bash
rm -rf build-debug
cmake -B build-debug -G Ninja -DCMAKE_BUILD_TYPE=Debug -DJUCE_PATH="$JUCE_PATH"
cmake --build build-debug --config Debug
```

---

## Release Build (optimized)

Use for testing performance or local distribution. Optimized binaries, smaller and faster. No debug symbols.

- **rm -rf build-release** — Clean previous release build
- **cmake -B build-release …** — Configure for Release
- **cmake --build …** — Build optimized binaries

```bash
rm -rf build-release
cmake -B build-release -G Ninja -DCMAKE_BUILD_TYPE=Release -DJUCE_PATH="$JUCE_PATH"
cmake --build build-release --config Release
```

---

## Universal Release Build (arm64 + x86_64, for distribution)

Use when preparing builds for distribution. Produces universal binaries that run on both Apple Silicon (arm64) and Intel Macs (x86_64).

- **DPLUGIN_DEV_MODE=OFF** — Disable dev-only features
- **UniversalBinary=ON** — Build both architectures
- **CMAKE_OSX_DEPLOYMENT_TARGET** — Minimum macOS version (10.13 = High Sierra)
- **-j$(sysctl -n hw.ncpu)** — Use all CPU cores for parallel build

```bash
rm -rf build-release
cmake -B build-release -DCMAKE_BUILD_TYPE=Release -DJUCE_PATH="$JUCE_PATH" -DPLUGIN_DEV_MODE=OFF -DUniversalBinary=ON -DCMAKE_OSX_DEPLOYMENT_TARGET="10.13"
cmake --build build-release --config Release -j$(sysctl -n hw.ncpu)
```

---

## Run Standalone (after Debug build)

Launches the standalone app built in Debug mode. Use for quick testing without a DAW.

```bash
./build-debug/AnalyzerPro_artefacts/Debug/Standalone/AnalyzerPro.app/Contents/MacOS/AnalyzerPro
```

---

## Run Standalone (after Release build)

Launches the standalone app built in Release mode. Same as above but uses optimized build.

```bash
./build-release/AnalyzerPro_artefacts/Release/Standalone/AnalyzerPro.app/Contents/MacOS/AnalyzerPro
```

---

## Incremental Build (no clean)

Use when you only changed source code and want to rebuild without re-running CMake. Skips the configure step.

**Debug** — Rebuild debug binaries:
```bash
cmake --build build-debug --config Debug
```

**Release** — Rebuild release binaries:
```bash
cmake --build build-release --config Release
```

---

## Use Build Script

Convenience script that runs the full release (universal) build. Requires `JUCE_PATH` to be set. Use when you prefer a one-liner.

```bash
./scripts/build_release.sh
```

---

## Plugin Output Paths

Where to find the built plugins after a successful build. Replace `*` with `Debug` or `Release` depending on your build type.

- **AU (Audio Unit)** — For Logic, GarageBand, Ableton, etc.
- **VST3** — For most DAWs (Cubase, Reaper, Pro Tools, etc.)
- **Standalone** — Standalone app (no host needed)

- **AU:** `build-*/AnalyzerPro_artefacts/*/AU/AnalyzerPro.component`
- **VST3:** `build-*/AnalyzerPro_artefacts/*/VST3/AnalyzerPro.vst3`
- **Standalone:** `build-*/AnalyzerPro_artefacts/*/Standalone/AnalyzerPro.app`

---

## Git Commands

### Status

Shows which files are modified, staged, or untracked. Run often to see your working tree state.

```bash
git status
```

### Stage changes

Add files to the staging area (index) before committing.

- **git add .** — Stage all changes in the current directory (recursive)
- **git add path/to/file** — Stage a specific file only

```bash
git add .
```
```bash
git add path/to/file
```

### Commit

Create a snapshot of staged changes with a message. Use clear, descriptive messages.

```bash
git commit -m "message"
```

### Push

Upload local commits to the remote repository.

- **git push** — Push current branch to its configured upstream
- **git push origin main** — Push explicitly to `main` on `origin`

```bash
git push
```
```bash
git push origin main
```

### Pull

Download and merge changes from the remote. Run before starting work to get latest changes.

```bash
git pull
```
```bash
git pull origin main
```

### Create branch

Create and switch to a new branch in one step. Use for features, fixes, or experiments.

```bash
git checkout -b branch-name
```

### Switch branch

Switch to an existing branch. Discard or stash uncommitted changes first if needed.

```bash
git checkout branch-name
```

### List branches

Show all branches. **-a** includes remote branches.

```bash
git branch -a
```

### Log

View commit history.

- **git log --oneline** — Compact one-line-per-commit view
- **git log -5** — Last 5 commits (full message)

```bash
git log --oneline
```
```bash
git log -5
```

### Discard unstaged changes

Revert working directory files to last committed state. **Warning:** Changes are lost permanently.

- **git checkout -- path** — Discard changes in one file
- **git restore .** — Discard all unstaged changes in current dir

```bash
git checkout -- path/to/file
```
```bash
git restore .
```

### Unstage

Remove a file from the staging area. File stays modified, just not staged for commit.

```bash
git restore --staged path/to/file
```

### Diff

View changes between states.

- **git diff** — Unstaged changes (working dir vs index)
- **git diff --staged** — Staged changes (index vs last commit)

```bash
git diff
```
```bash
git diff --staged
```

### Merge

Integrate another branch into the current branch. Run from the branch that should receive the changes (e.g. `main`).

```bash
git merge branch-name
```

### Clone

Download a repository from a remote URL into a new local folder. Run once per new checkout.

```bash
git clone https://github.com/user/repo.git
```
