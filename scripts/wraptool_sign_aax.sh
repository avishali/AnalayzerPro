#!/usr/bin/env bash
# Sign a built AAX bundle with Avid/PACE wraptool (macOS).
#
# CMake invokes this as a POST_BUILD step with the path to the AAX Mach-O
# binary inside the bundle (JUCE's TARGET_FILE for the AAX target).
#
# Credentials (never commit):
#   Local: copy scripts/.aax_wraptool.env.example to scripts/.aax_wraptool.env and fill in;
#           this script sources .aax_wraptool.env on macOS when the file exists.
#   CI: define the same AAX_WRAPTOOL_* names as protected secrets in the job environment.
#
# Required variables (env or after sourcing .aax_wraptool.env):
#   AAX_WRAPTOOL_ACCOUNT    — iLok account (wraptool --account)
#   AAX_WRAPTOOL_PASSWORD   — iLok password (wraptool --password)
#   AAX_WRAPTOOL_SIGNID     — e.g. "Developer ID Application: Your Name (TEAMID)" (--signid)
#
# Publisher (wraptool requires one of the following — see "wraptool sign" examples):
#   AAX_WRAPTOOL_WCGUID              — Fusion wrap config GUID → --wcguid
#   or both:
#   AAX_WRAPTOOL_CUSTOMERNUMBER      — PACE-issued customer number → --customernumber
#   AAX_WRAPTOOL_CUSTOMERNAME        — Company name → --customername
#
# Optional:
#   AAX_WRAPTOOL_ALLOW_SIGNING_SERVICE — set to 1 to pass --allowsigningservice. Per PACE help, the
#                                      signing service is used when **no signing iLok is plugged in**
#                                      (needs network, account, password). If a USB iLok is plugged in
#                                      but has no publisher signing cert, unplug it to force cloud, or
#                                      deposit the signing credential on the iLok (iLok LM → Synchronize).
#   AAX_WRAPTOOL_AUTOINSTALL — if non-empty, passed as --autoinstall <value> (e.g. on, off; License
#                             Support installer; see wraptool --help). Official templates may use on.
#   AAX_WRAPTOOL            — full path to wraptool (default: Fusion Current on macOS)
#   argv[2]                 — same as AAX_WRAPTOOL if set (CMake -DAAX_WRAPTOOL passes this)
#
# References: Avid AAX code signing + PACE Eden/Fusion wraptool documentation.

set -euo pipefail

mach_o="${1:?Usage: $0 <path-to-AAX-Mach-O-in-bundle> [wraptool-path]}"

if [[ "$(uname -s)" != "Darwin" ]]; then
  echo "wraptool_sign_aax.sh: only macOS is supported by this script." >&2
  exit 0
fi

_script_dir="$(cd "$(dirname "${BASH_SOURCE[0]:-$0}")" && pwd)"
_aax_env="${_script_dir}/.aax_wraptool.env"
if [[ -f "${_aax_env}" ]]; then
  set -a
  # shellcheck source=/dev/null
  source "${_aax_env}"
  set +a
fi

# Bundle root: .../Something.aaxplugin/Contents/MacOS/Something -> .../Something.aaxplugin
bundle="$(cd "$(dirname "${mach_o}")/../.." && pwd)"
if [[ ! -d "${bundle}/Contents" ]]; then
  echo "wraptool_sign_aax.sh: could not resolve AAX bundle from: ${mach_o}" >&2
  exit 1
fi

# Optional: second argument = full path to wraptool (CMake -DAAX_WRAPTOOL=...)
wrap_override="${2:-}"
if [[ -n "${wrap_override}" ]]; then
  WRAPTOOL="${wrap_override}"
else
  WRAPTOOL="${AAX_WRAPTOOL:-/Applications/PACEAntiPiracy/Eden/Fusion/Current/bin/wraptool}"
fi
if [[ ! -x "${WRAPTOOL}" ]]; then
  echo "wraptool_sign_aax.sh: wraptool not found or not executable: ${WRAPTOOL}" >&2
  echo "  Set AAX_WRAPTOOL to the full path to wraptool." >&2
  exit 1
fi

: "${AAX_WRAPTOOL_ACCOUNT:?Set AAX_WRAPTOOL_ACCOUNT (iLok email)}"
: "${AAX_WRAPTOOL_SIGNID:?Set AAX_WRAPTOOL_SIGNID (e.g. Developer ID Application: ... )}"
: "${AAX_WRAPTOOL_PASSWORD:?Set AAX_WRAPTOOL_PASSWORD in the environment for wraptool --password}"

_publisher_args=()
if [[ -n "${AAX_WRAPTOOL_WCGUID:-}" ]]; then
  _publisher_args=(--wcguid "${AAX_WRAPTOOL_WCGUID}")
elif [[ -n "${AAX_WRAPTOOL_CUSTOMERNUMBER:-}" && -n "${AAX_WRAPTOOL_CUSTOMERNAME:-}" ]]; then
  _publisher_args=(--customernumber "${AAX_WRAPTOOL_CUSTOMERNUMBER}" --customername "${AAX_WRAPTOOL_CUSTOMERNAME}")
else
  echo "wraptool_sign_aax.sh: set either AAX_WRAPTOOL_WCGUID or both AAX_WRAPTOOL_CUSTOMERNUMBER and AAX_WRAPTOOL_CUSTOMERNAME (PACE wraptool sign)." >&2
  exit 1
fi

_cloud_sign_args=()
case "${AAX_WRAPTOOL_ALLOW_SIGNING_SERVICE:-}" in
  1|yes|true|TRUE|Yes|YES|on|ON)
    _cloud_sign_args=(--allowsigningservice)
    echo "wraptool_sign_aax.sh: --allowsigningservice enabled. PACE uses cloud signing only when no signing iLok is plugged in; if you still get CouldNotFindSignerCredentials, unplug the USB iLok or install the publisher signing cert on it (iLok License Manager → Synchronize)." >&2
    ;;
esac

_autoinstall_args=()
if [[ -n "${AAX_WRAPTOOL_AUTOINSTALL:-}" ]]; then
  _autoinstall_args=(--autoinstall "${AAX_WRAPTOOL_AUTOINSTALL}")
fi

echo "wraptool_sign_aax.sh: signing bundle: ${bundle}"

# "${arr[@]}" with set -u errors when arr is empty; use ${arr[@]+"${arr[@]}"} for zero args.
"${WRAPTOOL}" sign --verbose \
  --account "${AAX_WRAPTOOL_ACCOUNT}" \
  --password "${AAX_WRAPTOOL_PASSWORD}" \
  ${_publisher_args[@]+"${_publisher_args[@]}"} \
  ${_cloud_sign_args[@]+"${_cloud_sign_args[@]}"} \
  ${_autoinstall_args[@]+"${_autoinstall_args[@]}"} \
  --dsig1-compat off \
  --signid "${AAX_WRAPTOOL_SIGNID}" \
  --in "${bundle}" \
  --out "${bundle}"

"${WRAPTOOL}" verify --verbose --in "${bundle}"

echo "wraptool_sign_aax.sh: sign + verify OK."
