#!/usr/bin/env bash
# Load scripts/.env when present (gitignored). Source from other scripts after SCRIPT_DIR
# is set to this directory (.../AnalyzerPro/scripts).
#
# If JUCE_PATH was already set in the environment before sourcing, it wins over .env.

if [[ -z "${SCRIPT_DIR:-}" ]]; then
  echo "source_repo_env.sh: SCRIPT_DIR must be set before sourcing" >&2
  return 1 2>/dev/null || exit 1
fi

if [[ ! -f "${SCRIPT_DIR}/.env" ]]; then
  return 0 2>/dev/null || exit 0
fi

if [[ "${JUCE_PATH+x}" == "x" ]]; then
  _repo_env_prev_juce="$JUCE_PATH"
else
  unset _repo_env_prev_juce
fi

set -a
# shellcheck source=/dev/null
source "${SCRIPT_DIR}/.env"
set +a

if [[ "${_repo_env_prev_juce+x}" == "x" ]]; then
  export JUCE_PATH="${_repo_env_prev_juce}"
  unset _repo_env_prev_juce
fi

return 0 2>/dev/null || exit 0
