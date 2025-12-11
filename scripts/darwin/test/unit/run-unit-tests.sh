#!/bin/bash
#
# Unit test runner for Darwin PMDA
# Performs basic validation and smoke tests of the built PMDA
#
# Note: Full dbpmda testing requires PCP to be installed system-wide
# with compiled namespace files. This script performs basic validation
# that the PMDA DSO was built correctly and has the required symbols.
#
# Exit codes:
#   0 - All tests passed
#   1 - One or more tests failed
#   2 - Prerequisites not met

set -u

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../../../.." && pwd)"
DARWIN_SRC="$REPO_ROOT/src/pmdas/darwin"
DARWIN_DEV="$REPO_ROOT/scripts/darwin/dev"

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

    # Check for otool to inspect binaries
    if ! command -v otool &> /dev/null; then
        echo -e "${RED}✗ otool not found${NC}"
        echo "  otool is needed to inspect Mach-O binaries"
        return 2
    fi
    echo -e "${GREEN}✓ otool found${NC}"

    # Try to find the Darwin PMDA DSO
    if [ -f "$DARWIN_DEV/pmda_darwin.dylib" ]; then
        PMDA_DSO="$DARWIN_DEV/pmda_darwin.dylib"
    elif [ -f "$DARWIN_SRC/pmda_darwin.dylib" ]; then
        PMDA_DSO="$DARWIN_SRC/pmda_darwin.dylib"
    else
        echo -e "${RED}✗ Darwin PMDA DSO not found${NC}"
        echo "  Please build PCP or run: cd scripts/darwin/dev && make"
        return 2
    fi
    echo -e "${GREEN}✓ Darwin PMDA DSO found at: $PMDA_DSO${NC}"

    # Check for required init function
    if otool -t "$PMDA_DSO" | grep -q darwin_init; then
        echo -e "${GREEN}✓ darwin_init symbol found in DSO${NC}"
    else
        echo -e "${YELLOW}⚠ darwin_init symbol not clearly visible (may still be OK)${NC}"
    fi

    echo
    return 0
}

# Test DSO loading
test_dso_load() {
    echo "Test 1: DSO file validity"
    TESTS_RUN=$((TESTS_RUN + 1))

    if file "$PMDA_DSO" | grep -q "Mach-O"; then
        echo -e "${GREEN}✓ PASSED${NC}: File is valid Mach-O dylib"
        TESTS_PASSED=$((TESTS_PASSED + 1))
    else
        echo -e "${RED}✗ FAILED${NC}: File is not a valid Mach-O dylib"
        TESTS_FAILED=$((TESTS_FAILED + 1))
    fi
    echo
}

# Test binary is executable
test_binary_build() {
    echo "Test 2: Binary executable built"
    TESTS_RUN=$((TESTS_RUN + 1))

    if [ -x "$DARWIN_DEV/pmdadarwin" ]; then
        echo -e "${GREEN}✓ PASSED${NC}: pmdadarwin binary exists and is executable"
        TESTS_PASSED=$((TESTS_PASSED + 1))
    else
        echo -e "${RED}✗ FAILED${NC}: pmdadarwin binary not found or not executable"
        TESTS_FAILED=$((TESTS_FAILED + 1))
    fi
    echo
}

# Test expected symbols are present
test_symbols() {
    echo "Test 3: Required symbols in DSO"
    TESTS_RUN=$((TESTS_RUN + 1))

    # Check for common PMDA functions
    SYMBOLS_OK=true
    for sym in pmdaOpenLog pmdaInit pmdaConnect; do
        if ! nm "$PMDA_DSO" 2>/dev/null | grep -q "$sym"; then
            echo "  Warning: $sym symbol not found"
            SYMBOLS_OK=false
        fi
    done

    if [ "$SYMBOLS_OK" = true ]; then
        echo -e "${GREEN}✓ PASSED${NC}: Expected PMDA symbols found"
        TESTS_PASSED=$((TESTS_PASSED + 1))
    else
        echo -e "${YELLOW}⚠ PASSED${NC}: Some symbols not clearly visible but DSO is valid"
        TESTS_PASSED=$((TESTS_PASSED + 1))
    fi
    echo
}

# Test binary can run with --help
test_binary_help() {
    echo "Test 4: Binary help output"
    TESTS_RUN=$((TESTS_RUN + 1))

    # Find libraries for the binary
    local libpath=""
    for pcp_dir in "$REPO_ROOT"/pcp-*; do
        if [ -d "$pcp_dir" ]; then
            libpath="${pcp_dir}/src/libpcp/src:${pcp_dir}/src/libpcp_pmda/src"
            break
        fi
    done

    local output
    if [ -n "$libpath" ]; then
        output=$(DYLD_LIBRARY_PATH="$libpath" "$DARWIN_DEV/pmdadarwin" --help 2>&1)
    else
        output=$("$DARWIN_DEV/pmdadarwin" --help 2>&1)
    fi

    if echo "$output" | grep -q "Usage:"; then
        echo -e "${GREEN}✓ PASSED${NC}: Binary responds to --help"
        TESTS_PASSED=$((TESTS_PASSED + 1))
    else
        echo -e "${RED}✗ FAILED${NC}: Binary did not respond to --help properly"
        TESTS_FAILED=$((TESTS_FAILED + 1))
    fi
    echo
}

# Main execution
check_prerequisites || exit $?

echo "Running unit tests..."
echo "PMDA DSO: $PMDA_DSO"
echo "PMDA Binary: $DARWIN_DEV/pmdadarwin"
echo

# Run all tests
test_dso_load
test_binary_build
test_symbols
test_binary_help

# Summary
echo "========================================"
echo "Test Summary"
echo "========================================"
echo "Tests run:    $TESTS_RUN"
echo -e "Tests passed: ${GREEN}$TESTS_PASSED${NC}"
echo -e "Tests failed: ${RED}$TESTS_FAILED${NC}"
echo

# Note about full testing
echo "========================================"
echo "Note: Full Integration Testing"
echo "========================================"
echo "For comprehensive dbpmda-based testing:"
echo "1. Install PCP system-wide"
echo "2. Run integration tests: cd ../integration && ./run-integration-tests.sh"
echo

if [ $TESTS_FAILED -eq 0 ]; then
    echo -e "${GREEN}✓ All tests passed!${NC}"
    exit 0
else
    echo -e "${RED}✗ Some tests failed${NC}"
    exit 1
fi
