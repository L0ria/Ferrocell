#!/usr/bin/env bash
# ---------------------------------------------------------------------------
# Ferrocell — host-side test runner (M2 issue #3, M3 issue #4, M5 issue #6).
#
# Builds and runs the PURE-core unit tests (tests/unit/*.cpp) with the system
# g++ — NO Arduino toolchain, NO FastLED, NO hardware — plus a small
# script-contract check (issue #25): the flash paths (flash.sh / build.sh
# --upload) must not pass flags arduino-cli `upload` does not accept.
# This is the same command the CI `core-tests` job runs (issue #7).
#
# Test targets (tests/unit/, each a self-contained binary with its own main()):
#   core_test      Color + ColorEngine   (M2)
#   settings_test  Settings defaults + normalize/clamping (M3)
#   api_test       Json + Api (the JSON API contract) (M5)
#
# All test binaries link against the pure core sources (Color, ColorEngine,
# Settings) which have zero hardware dependencies.
#
# Usage:
#   tests/run_tests.sh            # build + run all core tests
#   tests/run_tests.sh --keep     # keep the compiled binaries (./build/*)
#
# Exit code: 0 if all tests pass, non-zero otherwise (CI-friendly).
# ---------------------------------------------------------------------------
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

CXX="${CXX:-g++}"
CXXFLAGS="-std=c++17 -Wall -Wextra -O1"
OUT_DIR="${ROOT}/build"
KEEP=0
[ "${1:-}" = "--keep" ] && KEEP=1

# Pure core sources (no Arduino/FastLED) — linked into every test binary.
CORE_SRCS=(
  "${ROOT}/ferrocell/src/core/Color.cpp"
  "${ROOT}/ferrocell/src/core/ColorEngine.cpp"
  "${ROOT}/ferrocell/src/core/Settings.cpp"
  "${ROOT}/ferrocell/src/core/Json.cpp"
  "${ROOT}/ferrocell/src/core/Api.cpp"
)

# Test files to build + run (each has its own main()).
TESTS=(
  "core_test"
  "settings_test"
  "api_test"
)
TEST_DIR="${ROOT}/tests/unit"

mkdir -p "${OUT_DIR}"
echo "[tests] compiler : ${CXX} ($(${CXX} --version 2>/dev/null | head -1))"

overall_rc=0
for t in "${TESTS[@]}"; do
  BIN="${OUT_DIR}/${t}"
  echo "[tests] building : ${BIN}"
  "${CXX}" ${CXXFLAGS} -I "${ROOT}/ferrocell/src" \
    "${TEST_DIR}/${t}.cpp" \
    "${CORE_SRCS[@]}" \
    -o "${BIN}"

  echo "[tests] running  : ${BIN}"
  # Run the test binary, capturing its exit code. `set -e` is suspended for
  # the run so a failing test does not abort the whole harness — we want every
  # test to run and the overall result reported at the end.
  set +e
  ( cd "${ROOT}" && "${BIN}" )
  rc=$?
  set -e
  echo "[tests] ${t}: rc=${rc}"
  if [ "${rc}" -ne 0 ]; then
    overall_rc=${rc}
  fi
  if [ "${KEEP}" -eq 0 ]; then
    rm -f "${BIN}"
  fi
done

# ---------------------------------------------------------------------------
# Script contract (issue #25): arduino-cli 1.x `upload` has NO --libraries
# flag (it is compile-only). A stray `--libraries` in a flash path aborts the
# upload before it starts ("Error: unknown flag: --libraries"), so the upload
# command blocks must not carry it — while the compile block must still pin
# the vendored lib/ (arduino-cli does not auto-discover a sketch-local lib/).
# ---------------------------------------------------------------------------
echo ""
echo "[tests] script contract: flash paths use only upload-supported flags (issue #25)"
script_fail=0

# Print the arduino-cli <verb> command block of a script (from the verb line
# up to and including the sketch-dir argument line).
block_for() { # $1 = script, $2 = compile|upload
  # The command line looks like:  "${ARDUINO_CLI}" upload \   (note the ${} wrapper)
  # so match "ARDUINO_CLI" + the verb as separate tokens, then print until the
  # sketch-dir argument line.
  awk -v verb=" ${2} " '$0 ~ /ARDUINO_CLI/ && index($0, verb) {f=1} f {print} f && index($0, "SKETCH_DIR") {exit}' "$1"
}

check_script() { # $1 = description, $2 = 1 for ok / 0 for fail
  if [ "$2" = "1" ]; then
    echo "[tests]   ok   : $1"
  else
    echo "[tests]   FAIL : $1"
    script_fail=1
  fi
}

b="$(block_for "${ROOT}/flash.sh" upload)"
[ -n "${b}" ] && check_script "flash.sh: located the arduino-cli upload block" 1 \
              || check_script "flash.sh: located the arduino-cli upload block" 0
printf '%s\n' "${b}" | grep -q -- '--libraries' \
  && check_script "flash.sh: upload block has no --libraries (unknown flag for upload)" 0 \
  || check_script "flash.sh: upload block has no --libraries (unknown flag for upload)" 1

b="$(block_for "${ROOT}/build.sh" upload)"
[ -n "${b}" ] && check_script "build.sh: located the arduino-cli upload block" 1 \
              || check_script "build.sh: located the arduino-cli upload block" 0
printf '%s\n' "${b}" | grep -q -- '--libraries' \
  && check_script "build.sh: upload block has no --libraries (unknown flag for upload)" 0 \
  || check_script "build.sh: upload block has no --libraries (unknown flag for upload)" 1

b="$(block_for "${ROOT}/build.sh" compile)"
[ -n "${b}" ] && check_script "build.sh: located the arduino-cli compile block" 1 \
              || check_script "build.sh: located the arduino-cli compile block" 0
printf '%s\n' "${b}" | grep -q -- '--libraries' \
  && check_script "build.sh: compile block still pins the vendored lib/" 1 \
  || check_script "build.sh: compile block still pins the vendored lib/" 0

if [ "${script_fail}" -eq 1 ]; then
  overall_rc=1
fi

if [ "${overall_rc}" -eq 0 ]; then
  echo "[tests] ALL PASS"
else
  echo "[tests] FAILURES (rc=${overall_rc})"
fi
exit "${overall_rc}"
