#!/usr/bin/env bash
set -euo pipefail

# ==========================================
# AnalyzerPro / JUCE Template Bootstrap
# ==========================================

require() { command -v "$1" >/dev/null || { echo "Missing $1"; exit 1; }; }
require git
require python3

if [[ -n "$(git status --porcelain)" ]]; then
  echo "ERROR: Git working tree must be clean."
  exit 1
fi

echo "=== New JUCE Plugin Bootstrap ==="
read -rp "Plugin name (PascalCase, e.g. AnalyzerPro): " PLUGIN_NAME
read -rp "Company name (e.g. MyAudioCompany): " COMPANY_NAME
read -rp "4-char Plugin Code (e.g. AnPr): " PLUGIN_CODE
read -rp "4-char Manufacturer Code (e.g. AnPr): " MANUFACTURER_CODE

[[ ${#PLUGIN_CODE} -eq 4 ]] || { echo "PLUGIN_CODE must be 4 chars"; exit 1; }
[[ ${#MANUFACTURER_CODE} -eq 4 ]] || { echo "MANUFACTURER_CODE must be 4 chars"; exit 1; }

LOWER_NAME="$(echo "$PLUGIN_NAME" | tr '[:upper:]' '[:lower:]')"

git checkout -b "bootstrap/${LOWER_NAME}"

echo "→ Updating CMakeLists.txt"

python3 <<PY
from pathlib import Path

cmake = Path("CMakeLists.txt")
text = cmake.read_text()

replacements = {
    'set(PLUGIN_NAME "PluginTemplate")': f'set(PLUGIN_NAME "{PLUGIN_NAME}")',
    'set(COMPANY_NAME "YourCompany")': f'set(COMPANY_NAME "{COMPANY_NAME}")',
    'set(PLUGIN_CODE "Tmpl")': f'set(PLUGIN_CODE "{PLUGIN_CODE}")',
    'set(MANUFACTURER_CODE "Tmpl")': f'set(MANUFACTURER_CODE "{MANUFACTURER_CODE}")',
}

for old, new in replacements.items():
    if old not in text:
        raise RuntimeError(f"Expected line not found: {old}")
    text = text.replace(old, new)

cmake.write_text(text)
PY

echo "→ Renaming JUCE processor/editor classes"

python3 <<PY
from pathlib import Path

pairs = {
    "AnalayzerProAudioProcessor": f"{PLUGIN_NAME}AudioProcessor",
    "AnalayzerProAudioProcessorEditor": f"{PLUGIN_NAME}AudioProcessorEditor",
}

files = list(Path("Source").rglob("*.h")) + list(Path("Source").rglob("*.cpp"))

for f in files:
    t = f.read_text()
    for old, new in pairs.items():
        t = t.replace(old, new)
    f.write_text(t)
PY

git add CMakeLists.txt Source
git commit -m "chore: initialize ${PLUGIN_NAME} plugin identity"

echo "→ Building to validate rename"
cmake -S . -B build
cmake --build build

git commit --allow-empty -m "build: ${PLUGIN_NAME} initial build pass"

echo ""
echo "✅ ${PLUGIN_NAME} initialized successfully"
echo "Next: update README, CONTROL_IDS, DSP/UI"