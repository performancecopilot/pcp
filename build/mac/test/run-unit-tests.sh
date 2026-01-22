#!/bin/bash
# Run unit tests for both darwin and darwin_proc PMDAs
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../../.." && pwd)"

echo "=== Running all PMDA unit tests ==="
echo

echo "Darwin PMDA unit tests:"
cd "$REPO_ROOT/src/pmdas/darwin/test"
./run-unit-tests.sh
echo

echo "Darwin_proc PMDA unit tests:"
cd "$REPO_ROOT/src/pmdas/darwin_proc/test"
./run-unit-tests.sh

echo
echo "=== All unit tests complete ==="
