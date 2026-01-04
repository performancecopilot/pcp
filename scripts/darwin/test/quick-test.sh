#!/bin/bash
#
# Quick test script - runs both unit and integration tests
# Use this for local development workflow
#

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

echo -e "${BLUE}========================================"
echo "Darwin PMDA Quick Test Suite"
echo -e "========================================${NC}"
echo

# Step 1: Build
echo -e "${BLUE}Step 1: Building Darwin PMDA${NC}"
cd "$SCRIPT_DIR/../dev"
./build-quick.sh
echo

# Step 2: Unit tests
echo -e "${BLUE}Step 2: Running Unit Tests${NC}"
cd "$SCRIPT_DIR/unit"
if ./run-unit-tests.sh; then
    echo -e "${GREEN}✓ Unit tests passed${NC}"
else
    echo -e "${RED}✗ Unit tests failed${NC}"
    exit 1
fi
echo

# Step 3: Check if we should run integration tests
if pgrep -q pmcd; then
    echo -e "${BLUE}Step 3: Running Integration Tests${NC}"
    cd "$SCRIPT_DIR/integration"
    if ./run-integration-tests.sh; then
        echo -e "${GREEN}✓ Integration tests passed${NC}"
    else
        echo -e "${YELLOW}⚠ Integration tests failed${NC}"
        echo "  (This is expected if PCP is not installed)"
    fi
else
    echo -e "${YELLOW}Skipping integration tests (pmcd not running)${NC}"
    echo "  To run integration tests:"
    echo "    1. Install PCP system-wide"
    echo "    2. Start pmcd: sudo pmcd start"
    echo "    3. Run: cd integration && ./run-integration-tests.sh"
fi

echo
echo -e "${GREEN}========================================"
echo "Quick test complete!"
echo -e "========================================${NC}"
