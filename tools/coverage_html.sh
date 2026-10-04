#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT_FILE="${1:-${ROOT_DIR}/build/coverage/index.html}"

if [[ "${OUT_FILE}" != /* ]]; then
  OUT_FILE="${ROOT_DIR}/${OUT_FILE}"
fi

# Instrumentation is the ONLY thing that differs from the normal build; the C standard,
# include paths and warning set all come from the Makefile so they cannot drift apart.
COVERAGE_OPT='-O0 -g --coverage'

cd "${ROOT_DIR}"

if ! command -v gcovr >/dev/null 2>&1; then
  echo "Error: gcovr is not installed."
  echo "Install with: pip install gcovr"
  exit 1
fi

make clean >/dev/null
make build/tests/ctest OPT="${COVERAGE_OPT}" >/dev/null
./build/tests/ctest >/dev/null

mkdir -p "$(dirname "${OUT_FILE}")"

# Emit the HTML report and a text summary (line + branch) in a single gcovr pass, so
# the console output is not duplicated. gcovr's chatty "(INFO)" progress lines are
# filtered from stderr; warnings and errors still pass through and preserve the exit code.
echo "Coverage:"
gcovr -r "${ROOT_DIR}" \
  --filter "${ROOT_DIR}/src" \
  --html-details \
  --output "${OUT_FILE}" \
  --txt - \
  --txt-summary \
  2> >(grep -v '^(INFO)' >&2)

echo "Coverage HTML report written to: ${OUT_FILE}"