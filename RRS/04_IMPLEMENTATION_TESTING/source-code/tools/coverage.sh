#!/usr/bin/env bash
# Line and branch coverage for the engine (docs/code_coverage_report.md).
# Needs gcovr: pip install gcovr. Run from source-code/: ./tools/coverage.sh [build-dir]
set -euo pipefail
cd "$(dirname "$0")/.."
BUILD="${1:-build-coverage}"
OUT="../../../docs/coverage"

cmake -S . -B "$BUILD" -DCMAKE_BUILD_TYPE=Debug -DRRS_COVERAGE=ON
cmake --build "$BUILD" --parallel
find "$BUILD" -name '*.gcda' -delete
ctest --test-dir "$BUILD" --output-on-failure

# The TUI, the console demo and main() need a terminal; they are covered by the
# system tests in docs/system_validation_report.md instead.
mkdir -p "$OUT"
gcovr -r . "$BUILD" \
    --exclude 'tests/' --exclude 'src/tui/' --exclude 'src/demo/' --exclude 'src/main.c' \
    --exclude-throw-branches --exclude-unreachable-branches \
    --html-details "$OUT/index.html" --html-title "RRS Code and Branch Coverage" \
    --print-summary
