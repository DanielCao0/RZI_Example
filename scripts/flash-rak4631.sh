#!/usr/bin/env bash
# Flash build/app/zephyr/zephyr.hex to a RAK4631 over J-Link SWD.
# Run this script on the host with the debugger connected to the carrier-board SWD header.
set -euo pipefail

REPO="$(cd "$(dirname "$0")/.." && pwd)"
WORKSPACE="$(dirname "${REPO}")"
HEX="${WORKSPACE}/build/app/zephyr/zephyr.hex"

if [[ "${1:-}" == "-h" || "${1:-}" == "--help" ]]; then
  echo "Usage: $0 [--build]"
  echo "  --build  Run ./scripts/container.sh build before flashing"
  exit 0
fi

if [[ "${1:-}" == "--build" ]]; then
  "${REPO}/scripts/container.sh" build
fi

[[ -f "${HEX}" ]] || {
  echo "error: missing ${HEX}; run ./scripts/container.sh build first" >&2
  exit 1
}

command -v JLinkExe >/dev/null || {
  echo "error: JLinkExe was not found in PATH" >&2
  exit 1
}

script="$(mktemp --suffix=.jlink)"
trap 'rm -f "${script}"' EXIT
cat > "${script}" <<EOF
si 1
speed 4000
device nRF52840_xxAA
connect
halt
loadfile ${HEX}
r
g
exit
EOF

JLinkExe -nogui 1 -ExitOnError 1 -CommanderScript "${script}"
echo "done: ${HEX}"
