#!/bin/bash
#
# Rapid development test script for Darwin PMDA
# Builds the PMDA and tests it with dbpmda - no system installation required!
#
# Usage: ./dev-test.sh [test-commands-file]
#
# If no test-commands-file is provided, runs default tests.
# You can also create a .dbpmdarc file in this directory for custom tests.

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

echo $SCRIPT_DIR
echo $REPO_ROOT


# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Find the PCP build directory
BUILD_DIR=""
for dir in "$REPO_ROOT"/pcp-*; do
    if [ -d "$dir" ]; then
        BUILD_DIR="$dir"
        break
    fi
done

if [ -z "$BUILD_DIR" ]; then
    echo -e "${RED}Error: Cannot find Makepkgs build directory${NC}"
    echo "Please run: cd $REPO_ROOT && ./Makepkgs --verbose"
    exit 1
fi

# Check if local PCP environment is set up
if [ ! -f "$SCRIPT_DIR/local-pcp.conf" ]; then
    echo -e "${YELLOW}Local PCP environment not found. Running setup...${NC}"
    "$SCRIPT_DIR/setup-local-pcp.sh"
fi

# Set up environment
export PCP_CONF="$SCRIPT_DIR/local-pcp.conf"
export DYLD_LIBRARY_PATH="$BUILD_DIR/src/libpcp/src:$BUILD_DIR/src/libpcp_pmda/src"
DBPMDA="$BUILD_DIR/src/dbpmda/src/dbpmda"

echo "=== Darwin PMDA Development Test ==="
echo "Build dir: $BUILD_DIR"
echo "PCP_CONF: $PCP_CONF"
echo

# Step 1: Build
echo -e "${YELLOW}=== Building Darwin PMDA ===${NC}"
cd "$SCRIPT_DIR"
make clean 2>/dev/null || true
make

if [ ! -f "pmda_darwin.dylib" ]; then
    echo -e "${RED}Error: Build failed - pmda_darwin.dylib not found${NC}"
    exit 1
fi
echo -e "${GREEN}Build successful!${NC}"
echo

# Step 2: Test with dbpmda
echo -e "${YELLOW}=== Testing with dbpmda ===${NC}"

# Prepare test commands
if [ -n "$1" ] && [ -f "$1" ]; then
    # Use provided test file
    TEST_FILE="$1"
    echo "Using test commands from: $TEST_FILE"
elif [ -f "$SCRIPT_DIR/.dbpmdarc" ]; then
    # Use .dbpmdarc if it exists
    TEST_FILE="$SCRIPT_DIR/.dbpmdarc"
    echo "Using test commands from: .dbpmdarc"
else
    # Default test commands
    TEST_FILE=""
fi

if [ -n "$TEST_FILE" ]; then
    # Run with test file
    "$DBPMDA" -e < "$TEST_FILE"
else
    # Run default tests
    "$DBPMDA" -e << 'EOF'
open dso ./pmda_darwin.dylib darwin_init 78
getdesc on
status

# Test hardware inventory metrics
fetch hinv.ncpu
fetch hinv.physmem
fetch hinv.pagesize

# Test memory metrics
fetch mem.physmem
fetch mem.freemem

# Test kernel metrics
fetch kernel.all.hz
fetch kernel.uname.sysname
fetch kernel.uname.release
fetch kernel.uname.machine

# Test load average
fetch kernel.all.load

# Test CPU metrics (aggregated)
fetch kernel.all.cpu.user
fetch kernel.all.cpu.sys
fetch kernel.all.cpu.idle

quit
EOF
fi

echo
echo -e "${GREEN}=== All tests passed! ===${NC}"
echo
echo "Quick tips:"
echo "  - Edit source: vim ../../../src/pmdas/darwin/*.c"
echo "  - Re-test: ./dev-test.sh"
echo "  - Interactive: $DBPMDA (after setting PCP_CONF and DYLD_LIBRARY_PATH)"
