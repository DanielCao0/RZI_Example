#!/usr/bin/env bash
# RZI1 slot1 update for native RZI on RAK4631 (USB CDC or hardware UART).
# First-time MCUboot+app still needs J-Link (scripts/flash-rak4631.sh).
set -euo pipefail

REPO="$(cd "$(dirname "$0")/.." && pwd)"
WS="$(dirname "${REPO}")"
PORT="${1:-}"
FILE="${2:-${WS}/build/app/app/zephyr/zephyr.signed.bin}"

if [[ -z "$PORT" ]]; then
	echo "usage: $0 <port> [signed.bin]" >&2
	echo "  e.g. $0 /dev/ttyACM0 ${WS}/build/app/app/zephyr/zephyr.signed.bin" >&2
	exit 2
fi

if [[ ! -f "$FILE" ]]; then
	echo "error: missing $FILE; build the app first" >&2
	exit 1
fi

# Prefer the ArduinoCore copy if present (same protocol); else the RZI copy.
SCRIPT="${HOME}/ArduinoCore-zephyr-ws/ArduinoCore-zephyr/extra/upload-rzi-usb.py"
if [[ ! -f "$SCRIPT" ]]; then
	SCRIPT="${WS}/rzi/scripts/upload-rzi1.py"
fi

exec python3 "$SCRIPT" --port "$PORT" --type slot1 --wait-reboot 45 "$FILE"
