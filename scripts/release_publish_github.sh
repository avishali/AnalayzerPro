#!/bin/bash

set -euo pipefail

# Create git tag and GitHub release for AnalyzerPro.

TAG="${1:-v1.1.1}"
TITLE="${2:-AnalyzerPro ${TAG}}"
NOTES_FILE="${3:-docs/QA_AAX_V1_1_1.md}"

git fetch origin master
git checkout master
git pull --ff-only origin master

if git rev-parse "$TAG" >/dev/null 2>&1; then
  echo "Tag already exists locally: $TAG"
else
  git tag "$TAG"
fi

git push origin "$TAG"

if [[ -f "$NOTES_FILE" ]]; then
  gh release create "$TAG" --title "$TITLE" --notes-file "$NOTES_FILE"
else
  gh release create "$TAG" --title "$TITLE" --generate-notes
fi

echo "Release created: $TAG"
