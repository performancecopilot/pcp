#!/bin/bash
# Centralized orchestration: build + unit tests (both PMDAs) + integration tests
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../../.." && pwd)"

echo "=== Darwin PMDA Test Suite ==="
echo

# Step 1: Build
echo "Step 1: Building Darwin PMDAs..."
cd "$REPO_ROOT/dev/darwin/dev"
./build-quick.sh
echo

# Step 2: Unit tests - Darwin PMDA
echo "Step 2a: Running Darwin PMDA unit tests..."
cd "$REPO_ROOT/src/pmdas/darwin/test"
./run-unit-tests.sh
echo

# Step 3: Unit tests - Darwin_proc PMDA
echo "Step 2b: Running Darwin_proc PMDA unit tests..."
cd "$REPO_ROOT/src/pmdas/darwin_proc/test"
./run-unit-tests.sh
echo

# Step 4: Integration tests (conditional)
if pgrep -q pmcd; then
    echo "Step 3: Running integration tests (both PMDAs)..."
    cd "$REPO_ROOT/build/mac/test/integration"
    ./run-integration-tests.sh
else
    echo "Step 3: Skipping integration tests (pmcd not running)"
    echo "  To run integration tests: sudo launchctl load /Library/LaunchDaemons/org.pcp.pmcd.plist"
fi

echo
echo "=== Test suite complete ==="
