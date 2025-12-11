#!/bin/bash
#
# Unit test runner for Darwin PMDA
# Uses dbpmda to test the PMDA in isolation without requiring installation
#
# Exit codes:
#   0 - All tests passed
#   1 - One or more tests failed
#   2 - Prerequisites not met

set -u

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../../../.." && pwd)"
DARWIN_SRC="$REPO_ROOT/src/pmdas/darwin"
DARWIN_BUILD="$REPO_ROOT/build/darwin"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Test results
TESTS_RUN=0
TESTS_PASSED=0
TESTS_FAILED=0

echo "========================================"
echo "Darwin PMDA Unit Test Suite"
echo "========================================"
echo

# Check prerequisites
check_prerequisites() {
    echo "Checking prerequisites..."

    if ! command -v dbpmda &> /dev/null; then
        echo -e "${RED}✗ dbpmda not found${NC}"
        echo "  Please install PCP first"
        return 2
    fi
    echo -e "${GREEN}✓ dbpmda found${NC}"

    # Try to find the Darwin PMDA DSO
    if [ -f "$DARWIN_BUILD/pmda_darwin.dylib" ]; then
        PMDA_DSO="$DARWIN_BUILD/pmda_darwin.dylib"
    elif [ -f "$DARWIN_SRC/pmda_darwin.dylib" ]; then
        PMDA_DSO="$DARWIN_SRC/pmda_darwin.dylib"
    elif [ -f "$REPO_ROOT/scripts/darwin/dev/pmda_darwin.dylib" ]; then
        PMDA_DSO="$REPO_ROOT/scripts/darwin/dev/pmda_darwin.dylib"
    else
        echo -e "${RED}✗ Darwin PMDA DSO not found${NC}"
        echo "  Please build PCP or run: cd scripts/darwin/dev && make"
        return 2
    fi
    echo -e "${GREEN}✓ Darwin PMDA DSO found at: $PMDA_DSO${NC}"

    # Check for PMNS file
    if [ ! -f "$DARWIN_SRC/pmns" ]; then
        echo -e "${RED}✗ Darwin PMDA pmns file not found${NC}"
        return 2
    fi
    echo -e "${GREEN}✓ PMNS file found${NC}"

    echo
    return 0
}

# Run a single test file
run_test() {
    local test_file="$1"
    local test_name=$(basename "$test_file" .txt)

    TESTS_RUN=$((TESTS_RUN + 1))

    echo "Running: $test_name"

    # Create dbpmda command file with DSO loading
    local tmp_cmds=$(mktemp)
    cat > "$tmp_cmds" <<EOF
open dso $PMDA_DSO darwin_init 78
getdesc on
EOF
    cat "$test_file" >> "$tmp_cmds"
    echo "quit" >> "$tmp_cmds"

    # Run dbpmda
    local output=$(mktemp)
    if dbpmda -n "$DARWIN_SRC/pmns" -ie < "$tmp_cmds" > "$output" 2>&1; then
        # Check for errors in output
        if grep -q "Error:" "$output" || grep -q "Unknown metric" "$output"; then
            echo -e "${RED}✗ FAILED${NC}: Errors found in output"
            TESTS_FAILED=$((TESTS_FAILED + 1))
            cat "$output"
        else
            echo -e "${GREEN}✓ PASSED${NC}"
            TESTS_PASSED=$((TESTS_PASSED + 1))
        fi
    else
        echo -e "${RED}✗ FAILED${NC}: dbpmda exited with error"
        TESTS_FAILED=$((TESTS_FAILED + 1))
        cat "$output"
    fi

    rm -f "$tmp_cmds" "$output"
    echo
}

# Main execution
check_prerequisites || exit $?

echo "Running unit tests..."
echo "PMDA DSO: $PMDA_DSO"
echo "PMNS: $DARWIN_SRC/pmns"
echo

# Run all test files
for test_file in "$SCRIPT_DIR"/test-*.txt; do
    if [ -f "$test_file" ]; then
        run_test "$test_file"
    fi
done

# Summary
echo "========================================"
echo "Test Summary"
echo "========================================"
echo "Tests run:    $TESTS_RUN"
echo -e "Tests passed: ${GREEN}$TESTS_PASSED${NC}"
echo -e "Tests failed: ${RED}$TESTS_FAILED${NC}"
echo

if [ $TESTS_FAILED -eq 0 ]; then
    echo -e "${GREEN}✓ All tests passed!${NC}"
    exit 0
else
    echo -e "${RED}✗ Some tests failed${NC}"
    exit 1
fi
