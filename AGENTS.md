# Repository Guidelines

## Project Structure & Module Organization
- `Source/`: Core plugin code (processor/editor, analyzer engine, UI, control binding, hardware adapters).
- `ui_core/`: JUCE-free UI core library used by the plugin UI.
- `third_party/`: External dependencies (including the `melechdsp-hq` submodule).
- `docs/`: Architecture, specs, and workflow documents.
- `PROMPTS/`: Required prompt-driven workflow for non-trivial changes.
- `build/`, `build-xcode/`: Generated artifacts (do not edit by hand).

## Build, Test, and Development Commands
- `export JUCE_PATH=/path/to/JUCE`: Required SDK path for CMake/JUCE.
- `./build.sh [Debug|Release]`: Configure and build AU/VST3/Standalone artifacts via CMake.
- `./clean_build.sh`: Remove common build directories for a clean rebuild.
- `cmake -S . -B build-ninja -G Ninja -DCMAKE_BUILD_TYPE=Debug`: Manual configure (see `docs/BUILD_VERIFY.md`).
- `./scripts/check-no-warnings.sh`: Build and fail if warnings are detected.

## Coding Style & Naming Conventions
- C++17 (see `CMakeLists.txt`); prefer existing patterns in `Source/`.
- Indentation is 4 spaces; braces are placed on their own line.
- Types use PascalCase; functions and variables use lowerCamelCase.
- Keep RT-safe boundaries: engine/DSP code must avoid allocations on the audio thread and must not depend on UI logic.

## Testing Guidelines
- No dedicated unit-test suite is defined; use build verification instead.
- Run the zero-warnings gate (`./scripts/check-no-warnings.sh`) before submitting.
- Validate behavior in a host when changes touch DSP, UI, or parameter wiring.

## Commit & Pull Request Guidelines
- Commits should map 1:1 with a single prompt (see `PROMPTS/`).
- Reference the prompt file in commit messages (per `CONTRIBUTING.md`).
- Keep diffs minimal and scoped; avoid refactors outside the prompt.
- PRs should describe the prompt used, summarize the change, and note verification steps.

## Prompt-Driven Workflow (Required)
- Read `PROMPTS/README.txt` and `PROMPTS/PROMPTS_INDEX.md` before starting.
- If no prompt exists, create one from `PROMPTS/TEMPLATES/`.
- After completion, archive the prompt in `PROMPTS/99_ARCHIVE/` or prefix it with `DONE_`.
