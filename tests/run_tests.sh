#!/usr/bin/env bash
# ---------------------------------------------------------------------------
# Ferrocell — host-side test runner (M2, issue #3).
#
# Builds and runs the PURE-core unit tests (Color + ColorEngine) with the
# system g++ — NO Arduino toolchain, NO hardware. This is the same command
# the M6 CI job will run.
#
# Usage:
#   tests/run_tests.sh            # build + run the core tests
#   tests/run_tests.sh --keep     # keep the compiled binary (./build/core_test)
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

BIN="${OUT_DIR}/core_test"
mkdir -p "${OUT_DIR}"

echo "[tests] compiler : ${CXX} ($(${CXX} --version 2>/dev/null | head -1))"
echo "[tests] building : ${BIN}"
"${CXX}" ${CXXFLAGS} -I "${ROOT}/ferrocell/src" \
  "${ROOT}/tests/core_test.cpp" \
  "${ROOT}/ferrocell/src/core/Color.cpp" \
  "${ROOT}/ferrocell/src/core/ColorEngine.cpp" \
  -o "${BIN}"

echo "[tests] running  : ${BIN}"
if [ "${KEEP}" -eq 0 ]; then
  # Run from a neutral cwd; clean up the binary afterwards.
  ( cd "${ROOT}" && "${BIN}" )
  rc=$?
  rm -f "${BIN}"
  exit "${rc}"
else
  ( cd "${ROOT}" && "${BIN}" )
fi
