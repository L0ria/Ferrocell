#!/usr/bin/env bash
# ---------------------------------------------------------------------------
# Ferrocell — build the ESP32-S3-Zero firmware (arduino-cli)
#
# Standalone, idempotent build helper mirroring a-i-a-d/Tankie. It:
#   1. ensures arduino-cli is available,
#   2. registers the esp32 board index and installs the PINNED core (3.3.12),
#   3. installs the pinned external libraries (FastLED 3.10.8, ElegantOTA
#      4.0.0) — the ESP-Wifi-Config dependency is vendored in
#      lib/ESP-Wifi-Config (v2.2.6),
#   4. compiles the `ferrocell` sketch for the Waveshare ESP32-S3-Zero FQBN,
#   5. optionally flashes (--upload) to the first detected ESP32.
#
# Usage:
#   ./build.sh                    # build only (default)
#   ./build.sh --upload           # build + flash to the first ESP32 on serial
#   ./build.sh --port /dev/ttyACM0   # flash to a specific port
#   ./build.sh --clean            # wipe build/ + the global sketch cache first
#   ARDUINO_JOBS=<n> ./build.sh   # override parallel compile jobs (0 = auto)
#
# Output:
#   build/ferrocell.ino.bin       (the application firmware image)
#
# Pins (single source of truth for the CLI build):
#   core   esp32:esp32 @ 3.3.12
#   FQBN   esp32:esp32:waveshare_esp32_s3_zero:PartitionScheme=otanofs
#   FastLED 3.10.8
#   ElegantOTA 4.0.0
#   ESP-Wifi-Config 2.2.6 (vendored in lib/)
# ---------------------------------------------------------------------------
set -euo pipefail

# --- pinned, known-good versions -------------------------------------------
CORE_PKG="esp32:esp32"
CORE_VERSION="3.3.12"
# Partition scheme: otanofs = two 1.94 MiB OTA slots (app0/app1), NO SPIFFS
# (issue #16, Q1: we mount no filesystem at runtime — the UI is PROGMEM-embedded
# and settings live in NVS — so the 1.5 MB SPIFFS of the `default` scheme was
# dead weight; otanofs gives ~836 KB more headroom per OTA slot for ElegantOTA.)
FQBN="esp32:esp32:waveshare_esp32_s3_zero:PartitionScheme=otanofs"
FASTLED_VERSION="3.10.8"
ELEGANTOTA_VERSION="4.0.0"
ARDUINO_CLI_VERSION="1.5.1"
ESP32_INDEX_URL="https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json"

# --- parallel-job budget (Tankie pattern: never OOM a container) ------------
# A single xtensa C++ compile (cc1plus) can use ~100-300 MB, and arduino-cli
# also runs the assembler/linker concurrently. Reserve 512 MB per job so the
# total stays inside the cgroup memory limit. Read the cgroup limit first
# (v2 memory.max, then v1), falling back to /proc/meminfo only when unset —
# inside a container /proc/meminfo reports the HOST's RAM and would OOM.
PER_JOB_KB=524288
MIN_JOBS=1
MAX_JOBS=32

effective_mem_kb() {
  local kb=""
  if [ -r /sys/fs/cgroup/memory.max ]; then
    kb="$(cat /sys/fs/cgroup/memory.max 2>/dev/null || true)"
  fi
  if { [ -z "${kb}" ] || [ "${kb}" = "max" ]; } && [ -r /sys/fs/cgroup/memory/memory.limit_in_bytes ]; then
    local v1
    v1="$(cat /sys/fs/cgroup/memory/memory.limit_in_bytes 2>/dev/null || true)"
    if [ -n "${v1}" ] && [ "${v1}" -lt 1000000000000 ] 2>/dev/null; then
      kb="${v1}"
    fi
  fi
  if [ -n "${kb}" ] && [ "${kb}" != "max" ] && [ "${kb}" -gt 0 ] 2>/dev/null; then
    echo $(( kb / 1024 )); return
  fi
  awk '/MemTotal/ {print $2}' /proc/meminfo 2>/dev/null
}

JOBS="${ARDUINO_JOBS:-0}"
if [ "${JOBS}" -eq 0 ]; then
  MEM_KB="$(effective_mem_kb)"; MEM_KB="${MEM_KB:-0}"
  if [ "${MEM_KB}" -gt 0 ]; then
    JOBS=$(( MEM_KB / PER_JOB_KB ))
    [ "${JOBS}" -lt "${MIN_JOBS}" ] && JOBS="${MIN_JOBS}"
    [ "${JOBS}" -gt "${MAX_JOBS}" ] && JOBS="${MAX_JOBS}"
  fi
  NCPU="$(nproc 2>/dev/null || echo 1)"
  [ "${JOBS}" -gt "${NCPU}" ] && JOBS="${NCPU}"
fi

# --- paths ------------------------------------------------------------------
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SKETCH_DIR="${SCRIPT_DIR}/ferrocell"
OUT_DIR="${SCRIPT_DIR}/build"
BIN="${OUT_DIR}/ferrocell.ino.bin"

# --- argument parsing -------------------------------------------------------
UPLOAD=0
CLEAN=0
PORT=""
for arg in "$@"; do
  case "${arg}" in
    --upload)  UPLOAD=1 ;;
    --clean)   CLEAN=1 ;;
    --port)    : ;;   # value consumed by the next iteration
    --port=*)  PORT="${arg#--port=}" ;;
    -h|--help) grep '^# ' "$0" | sed 's/^# //'; exit 0 ;;
    -*)        echo "unknown flag: ${arg}" >&2; exit 2 ;;
    *)         [ "${arg}" = "--port" ] && continue; PORT="${arg}" ;;
  esac
done

log()  { printf '\033[1;32m[build]\033[0m %s\n' "$*"; }
warn() { printf '\033[1;33m[build]\033[0m %s\n' "$*" >&2; }
die()  { printf '\033[1;31m[build]\033[0m %s\n' "$*" >&2; exit 1; }

# --- 0. locate / install arduino-cli ----------------------------------------
ARDUINO_CLI="$(command -v arduino-cli || true)"
if [ -z "${ARDUINO_CLI}" ]; then
  # Honour a local install (e.g. /root/bin/arduino-cli) before downloading.
  if [ -x /root/bin/arduino-cli ]; then
    ARDUINO_CLI=/root/bin/arduino-cli
  else
    log "arduino-cli not found — installing ${ARDUINO_CLI_VERSION} to /tmp/arduino-cli …"
    mkdir -p /tmp/arduino-cli
    curl -fsSL -o /tmp/arduino-cli.tar.gz \
      "https://github.com/arduino/arduino-cli/releases/download/v${ARDUINO_CLI_VERSION}/arduino-cli_${ARDUINO_CLI_VERSION}_Linux_64bit.tar.gz"
    tar -xzf /tmp/arduino-cli.tar.gz -C /tmp/arduino-cli
    ARDUINO_CLI=/tmp/arduino-cli/arduino-cli
  fi
fi
log "arduino-cli: ${ARDUINO_CLI} ($("${ARDUINO_CLI}" version 2>/dev/null | head -1))"

# --- 1. board index + pinned core -------------------------------------------
if ! "${ARDUINO_CLI}" config get board_manager.additional_urls 2>/dev/null | grep -qF "${ESP32_INDEX_URL}"; then
  log "adding the esp32 board index …"
  "${ARDUINO_CLI}" config add board_manager.additional_urls "${ESP32_INDEX_URL}" || true
fi
"${ARDUINO_CLI}" core update-index >/dev/null 2>&1 || warn "core update-index reported a warning (continuing)"

INSTALLED="$("${ARDUINO_CLI}" core list 2>/dev/null | awk '$1=="esp32:esp32"{print $2}')"
if [ "${INSTALLED}" != "${CORE_VERSION}" ]; then
  log "installing ${CORE_PKG}@${CORE_VERSION} (installed: ${INSTALLED:-none}) …"
  "${ARDUINO_CLI}" core install "${CORE_PKG}@${CORE_VERSION}"
else
  log "core ${CORE_PKG}@${CORE_VERSION} already installed"
fi

# --- 2. pinned external library (FastLED) -----------------------------------
# ESP-Wifi-Config is vendored in lib/ESP-Wifi-Config (v2.2.6) — it is NOT a
# registry lib, so there is nothing to `lib install`. It is passed to the
# compile explicitly via `--libraries "${SCRIPT_DIR}/lib"` (arduino-cli does
# NOT auto-discover a sketch-local lib/ dir; only the legacy user dir).
if [ ! -f "${SCRIPT_DIR}/lib/ESP-Wifi-Config/library.properties" ]; then
  die "vendored lib missing: lib/ESP-Wifi-Config (expected v2.2.6)"
fi
if ! "${ARDUINO_CLI}" lib list 2>/dev/null | awk '{print $1}' | grep -qx "FastLED"; then
  log "installing FastLED@${FASTLED_VERSION} …"
  "${ARDUINO_CLI}" lib update-index >/dev/null 2>&1 || true
  "${ARDUINO_CLI}" lib install "FastLED@${FASTLED_VERSION}"
else
  log "FastLED already installed"
fi

# --- 2b. pinned external library (ElegantOTA) ------------------------------
# OTA updates over WiFi (issue #16): the /update portal + /ota/* routes on the
# same WebServer UiServer owns (port 80, STA + AP). Zero non-core dependencies;
# the portal is a PROGMEM array (no filesystem needed — compatible with the
# PROGMEM-embedded UI).
if ! "${ARDUINO_CLI}" lib list 2>/dev/null | awk '{print $1}' | grep -qx "ElegantOTA"; then
  log "installing ElegantOTA@${ELEGANTOTA_VERSION} …"
  "${ARDUINO_CLI}" lib update-index >/dev/null 2>&1 || true
  "${ARDUINO_CLI}" lib install "ElegantOTA@${ELEGANTOTA_VERSION}"
else
  log "ElegantOTA already installed"
fi

# --- 3. preflight -----------------------------------------------------------
[ -f "${SKETCH_DIR}/ferrocell.ino" ] || die "sketch not found: ${SKETCH_DIR}/ferrocell.ino"
if [ "${CLEAN}" -eq 1 ]; then
  log "--clean: wiping ${OUT_DIR} and the global sketch cache"
  rm -rf "${OUT_DIR}"
  rm -rf "${HOME}/.cache/arduino/sketches" 2>/dev/null || true
fi

# --- 4. compile -------------------------------------------------------------
log "building ${FQBN} (jobs: ${JOBS}) …"
log "sketch: ${SKETCH_DIR}"
mkdir -p "${OUT_DIR}"
"${ARDUINO_CLI}" compile \
  --fqbn "${FQBN}" \
  --jobs "${JOBS}" \
  --libraries "${SCRIPT_DIR}/lib" \
  --export-binaries \
  --output-dir "${OUT_DIR}" \
  "${SKETCH_DIR}"
[ -f "${BIN}" ] || die "build did not produce ${BIN}"
log "built $(stat -c%s "${BIN}") bytes -> ${BIN}"

# --- 5. optional flash ------------------------------------------------------
if [ "${UPLOAD}" -eq 1 ]; then
  if [ -z "${PORT}" ]; then
    log "detecting an ESP32 serial port …"
    PORT="$("${ARDUINO_CLI}" board list 2>/dev/null | awk 'NR>1 && /ESP32|S3|tty/ {print $1; exit}')"
    [ -n "${PORT}" ] || die "no ESP32 detected — pass --port /dev/ttyACM0"
  fi
  log "flashing to ${PORT} …"
  "${ARDUINO_CLI}" upload --fqbn "${FQBN}" -p "${PORT}" --libraries "${SCRIPT_DIR}/lib" --input-dir "${OUT_DIR}" "${SKETCH_DIR}"
  log "flash complete"
fi

log "done."
