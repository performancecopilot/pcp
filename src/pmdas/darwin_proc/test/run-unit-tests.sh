#!/bin/bash
# Unit test runner for Darwin_proc PMDA
set -u

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../../../.." && pwd)"
DARWIN_PROC_SRC="$REPO_ROOT/src/pmdas/darwin_proc"
DARWIN_DEV="$REPO_ROOT/dev/darwin/dev"

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
NC='\033[0m'

# Test results
TESTS_RUN=0
TESTS_PASSED=0
TESTS_FAILED=0

echo "========================================"
echo "Darwin_proc PMDA Unit Test Suite"
echo "========================================"
echo

# Check prerequisites and find DSO
check_prerequisites() {
    echo "Checking prerequisites..."

    if ! command -v otool &> /dev/null; then
        echo -e "${RED}✗ otool not found${NC}"
        return 2
    fi
    echo -e "${GREEN}✓ otool found${NC}"

    # Try to find the Darwin_proc PMDA DSO
    if [ -f "$DARWIN_PROC_SRC/pmda_proc.dylib" ]; then
        PMDA_DSO="$DARWIN_PROC_SRC/pmda_proc.dylib"
    else
        # Check Makepkgs build location
        for pcp_dir in "$REPO_ROOT"/pcp-*; do
            if [ -f "$pcp_dir/src/pmdas/darwin_proc/pmda_proc.dylib" ]; then
                PMDA_DSO="$pcp_dir/src/pmdas/darwin_proc/pmda_proc.dylib"
                break
            fi
        done

        if [ -z "${PMDA_DSO:-}" ]; then
            echo -e "${RED}✗ Darwin_proc PMDA DSO not found${NC}"
            echo "  Please build PCP first: ./Makepkgs"
            return 2
        fi
    fi
    echo -e "${GREEN}✓ Darwin_proc PMDA DSO found at: $PMDA_DSO${NC}"

    return 0
}

# Test 1: DSO is valid
test_dso_valid() {
    TESTS_RUN=$((TESTS_RUN + 1))
    echo "Test 1: Validate DSO is valid Mach-O dylib..."

    if otool -L "$PMDA_DSO" | grep -q "pmda_proc.dylib"; then
        echo -e "${GREEN}✓ Test 1 passed${NC}"
        TESTS_PASSED=$((TESTS_PASSED + 1))
    else
        echo -e "${RED}✗ Test 1 failed${NC}"
        TESTS_FAILED=$((TESTS_FAILED + 1))
    fi
}

# Test 2: Binary exists
test_binary_exists() {
    TESTS_RUN=$((TESTS_RUN + 1))
    echo "Test 2: Check pmdaproc binary exists..."

    BINARY=""
    if [ -f "$DARWIN_PROC_SRC/pmdaproc" ]; then
        BINARY="$DARWIN_PROC_SRC/pmdaproc"
    else
        for pcp_dir in "$REPO_ROOT"/pcp-*; do
            if [ -f "$pcp_dir/src/pmdas/darwin_proc/pmdaproc" ]; then
                BINARY="$pcp_dir/src/pmdas/darwin_proc/pmdaproc"
                break
            fi
        done
    fi

    if [ -n "$BINARY" ] && [ -x "$BINARY" ]; then
        echo -e "${GREEN}✓ Test 2 passed: $BINARY${NC}"
        TESTS_PASSED=$((TESTS_PASSED + 1))
    else
        echo -e "${RED}✗ Test 2 failed: pmdaproc binary not found or not executable${NC}"
        TESTS_FAILED=$((TESTS_FAILED + 1))
    fi
}

# Test 3: Required symbols
test_required_symbols() {
    TESTS_RUN=$((TESTS_RUN + 1))
    echo "Test 3: Check for required PMDA symbols..."

    MISSING_SYMBOLS=0
    for symbol in pmdaOpenLog pmdaInit pmdaConnect; do
        if ! nm "$PMDA_DSO" | grep -q "$symbol"; then
            echo -e "${RED}  ✗ Missing symbol: $symbol${NC}"
            MISSING_SYMBOLS=$((MISSING_SYMBOLS + 1))
        fi
    done

    if [ $MISSING_SYMBOLS -eq 0 ]; then
        echo -e "${GREEN}✓ Test 3 passed${NC}"
        TESTS_PASSED=$((TESTS_PASSED + 1))
    else
        echo -e "${RED}✗ Test 3 failed: $MISSING_SYMBOLS required symbols missing${NC}"
        TESTS_FAILED=$((TESTS_FAILED + 1))
    fi
}

# Test 4: Binary responds to --help
test_binary_help() {
    TESTS_RUN=$((TESTS_RUN + 1))
    echo "Test 4: Check pmdaproc binary responds to --help..."

    BINARY=""
    if [ -f "$DARWIN_PROC_SRC/pmdaproc" ]; then
        BINARY="$DARWIN_PROC_SRC/pmdaproc"
    else
        for pcp_dir in "$REPO_ROOT"/pcp-*; do
            if [ -f "$pcp_dir/src/pmdas/darwin_proc/pmdaproc" ]; then
                BINARY="$pcp_dir/src/pmdas/darwin_proc/pmdaproc"
                break
            fi
        done
    fi

    if [ -n "$BINARY" ] && "$BINARY" --help 2>&1 | grep -qi "usage"; then
        echo -e "${GREEN}✓ Test 4 passed${NC}"
        TESTS_PASSED=$((TESTS_PASSED + 1))
    else
        echo -e "${RED}✗ Test 4 failed${NC}"
        TESTS_FAILED=$((TESTS_FAILED + 1))
    fi
}

# Run tests
if ! check_prerequisites; then
    exit 2
fi

test_dso_valid
test_binary_exists
test_required_symbols
test_binary_help

# Summary
echo
echo "========================================"
echo "Test Summary"
echo "========================================"
echo "Tests run:    $TESTS_RUN"
echo "Tests passed: $TESTS_PASSED"
echo "Tests failed: $TESTS_FAILED"
echo

if [ $TESTS_FAILED -gt 0 ]; then
    echo -e "${RED}✗ Some tests failed${NC}"
    exit 1
else
    echo -e "${GREEN}✓ All tests passed${NC}"
    exit 0
fi
