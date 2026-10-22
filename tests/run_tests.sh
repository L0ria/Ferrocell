#!/usr/bin/env bash
# ---------------------------------------------------------------------------
# Ferrocell — host-side test runner (M2 issue #3, M3 issue #4, M5 issue #6).
#
# Builds and runs the PURE-core unit tests with the system g++ — NO Arduino
# toolchain, NO FastLED, NO hardware. This is the same command the M6 CI
# `core-tests` job runs.
#
# Test targets (each is a self-contained binary with its own main()):
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

mkdir -p "${OUT_DIR}"
echo "[tests] compiler : ${CXX} ($(${CXX} --version 2>/dev/null | head -1))"

overall_rc=0
for t in "${TESTS[@]}"; do
  BIN="${OUT_DIR}/${t}"
  echo "[tests] building : ${BIN}"
  "${CXX}" ${CXXFLAGS} -I "${ROOT}/ferrocell/src" \
    "${ROOT}/tests/${t}.cpp" \
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

if [ "${overall_rc}" -eq 0 ]; then
  echo "[tests] ALL PASS"
else
  echo "[tests] FAILURES (rc=${overall_rc})"
fi
exit "${overall_rc}"
