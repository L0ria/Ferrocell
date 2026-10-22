#!/usr/bin/env bash
# ---------------------------------------------------------------------------
# Ferrocell — flash the already-built image (arduino-cli, flash-only)
#
# Companion to build.sh (issue #19): it does NOT compile — it uploads the
# artifacts build.sh exported to build/ (bootloader, partitions, boot_app0,
# app). Use it for quick re-flashes, baud-rate experiments (the ESP32-S3-Zero
# can brownout from the 5 V rail at higher speeds), or a different chip.
#
# Parameters:
#   --port <dev>    serial device (default: first ESP32/tty found via
#                   `arduino-cli board list`; PORT env var also honored)
#   --speed <baud>  flash baud rate (default: 115200 — brownout-safe;
#                   SPEED env var also honored)
#   --fqbn <fqbn>   board/chip type (default: the S3-Zero FQBN build.sh uses;
#                   FQBN env var also honored)
#   --erase         erase ALL flash (factory reset) instead of uploading
#
# Usage:
#   ./flash.sh                            # flash build/ to the first ESP32 @ 115200
#   ./flash.sh --port /dev/ttyACM0        # specific port
#   ./flash.sh --speed 921600             # faster link (if the rail can take it)
#   ./flash.sh --fqbn esp32:esp32:esp32s3# different chip/board
#   ./flash.sh --erase                    # wipe all flash (factory reset)
#
# Requires: a prior ./build.sh (build/ferrocell.ino.bin must exist) and the
# pinned core installed (build.sh does that).
# ---------------------------------------------------------------------------
set -euo pipefail

# --- defaults (overridable via flag > env > default) ------------------------
DEFAULT_FQBN="esp32:esp32:waveshare_esp32_s3_zero:PartitionScheme=otanofs"
FQBN="${FQBN:-${DEFAULT_FQBN}}"
SPEED="${SPEED:-115200}"
PORT="${PORT:-}"

# --- paths ------------------------------------------------------------------
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SKETCH_DIR="${SCRIPT_DIR}/ferrocell"
OUT_DIR="${SCRIPT_DIR}/build"
BIN="${OUT_DIR}/ferrocell.ino.bin"

# --- argument parsing -------------------------------------------------------
ERASE=0
prev=""
for arg in "$@"; do
  case "${prev}" in
    --port)  PORT="${arg}"; prev=""; continue ;;
    --speed) SPEED="${arg}"; prev=""; continue ;;
    --fqbn)  FQBN="${arg}"; prev=""; continue ;;
  esac
  case "${arg}" in
    --port)   prev="--port" ;;
    --speed)  prev="--speed" ;;
    --fqbn)   prev="--fqbn" ;;
    --port=*)  PORT="${arg#--port=}" ;;
    --speed=*) SPEED="${arg#--speed=}" ;;
    --fqbn=*)  FQBN="${arg#--fqbn=}" ;;
    --erase)   ERASE=1 ;;
    -h|--help) grep '^# ' "$0" | sed 's/^# //'; exit 0 ;;
    -*)        echo "unknown flag: ${arg}" >&2; exit 2 ;;
    *)         echo "unexpected argument: ${arg}" >&2; exit 2 ;;
  esac
done
# A value flag left dangling at end-of-args is a usage error.
[ -z "${prev}" ] || { echo "missing value for ${prev}" >&2; exit 2; }

log() { printf '\033[1;32m[flash]\033[0m %s\n' "$*"; }
die() { printf '\033[1;31m[flash]\033[0m %s\n' "$*" >&2; exit 1; }

# --- locate arduino-cli (build.sh installs it if missing) -------------------
ARDUINO_CLI="$(command -v arduino-cli || true)"
if [ -z "${ARDUINO_CLI}" ] && [ -x /root/bin/arduino-cli ]; then
  ARDUINO_CLI=/root/bin/arduino-cli
fi
if [ -z "${ARDUINO_CLI}" ] && [ -x /tmp/arduino-cli/arduino-cli ]; then
  ARDUINO_CLI=/tmp/arduino-cli/arduino-cli
fi
[ -n "${ARDUINO_CLI}" ] || die "arduino-cli not found — run ./build.sh first (it locates/installs it)"

# --- resolve the serial port -------------------------------------------------
if [ -z "${PORT}" ]; then
  log "detecting an ESP32 serial port …"
  PORT="$( "${ARDUINO_CLI}" board list 2>/dev/null | awk 'NR>1 && /ESP32|S3|tty/ {print $1; exit}' )"
  [ -n "${PORT}" ] || die "no ESP32 detected — pass --port /dev/ttyACM0"
fi

# --- erase (simple, no image needed) ----------------------------------------
if [ "${ERASE}" -eq 1 ]; then
  log "erasing all flash on ${PORT} (factory reset) …"
  "${ARDUINO_CLI}" board erase -p "${PORT}" --fqbn "${FQBN}"
  log "erase complete"
  exit 0
fi

# --- preflight: the image must exist (A3: die with a message) ----------------
[ -f "${BIN}" ] || die "no image available (missing ${BIN}) — please run ./build.sh first"
log "flashing $(stat -c%s "${BIN}") bytes from ${BIN}"
log "port: ${PORT}  speed: ${SPEED}  fqbn: ${FQBN}"

# --- upload the exported artifacts (no recompile) ----------------------------
# --upload-property upload.speed=<baud> is honored by the esptool recipe in
# the esp32 core (platform.txt: --baud {upload.speed}), so the baud rate is
# a plain board-property override — no core patching needed.
"${ARDUINO_CLI}" upload \
  --fqbn "${FQBN}" \
  -p "${PORT}" \
  --upload-property "upload.speed=${SPEED}" \
  --libraries "${SCRIPT_DIR}/lib" \
  --input-dir "${OUT_DIR}" \
  "${SKETCH_DIR}"

log "flash complete"
