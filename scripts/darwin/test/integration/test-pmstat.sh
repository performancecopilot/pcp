#!/bin/bash
#
# Detailed pmstat validation test
# Ensures pmstat shows macOS-specific metrics correctly
#

set -u

RED='\033[0;31m'
GREEN='\033[0;32m'
NC='\033[0m'

echo "Testing pmstat with Darwin PMDA..."

# Run pmstat for 2 samples
# Use -h localhost to force TCP connection (more reliable in CI environments)
output=$(timeout 10 pmstat -h localhost -t 1 -s 2 2>&1)
exit_code=$?

if [ $exit_code -ne 0 ]; then
    echo -e "${RED}✗ pmstat failed to run${NC}"
    echo "$output"
    exit 1
fi

echo "pmstat output:"
echo "---"
echo "$output"
echo "---"
echo

# Validate output contains expected columns
checks_passed=0
checks_failed=0

# Check for load average column
if echo "$output" | grep -qE 'loadavg.*[0-9]+\.[0-9]+'; then
    echo -e "${GREEN}✓ Load average present${NC}"
    checks_passed=$((checks_passed + 1))
else
    echo -e "${RED}✗ Load average missing${NC}"
    checks_failed=$((checks_failed + 1))
fi

# Check for memory columns
if echo "$output" | grep -qE 'memory.*[0-9]+'; then
    echo -e "${GREEN}✓ Memory stats present${NC}"
    checks_passed=$((checks_passed + 1))
else
    echo -e "${RED}✗ Memory stats missing${NC}"
    checks_failed=$((checks_failed + 1))
fi

# Check for disk I/O
if echo "$output" | grep -qE 'disk.*[0-9]+'; then
    echo -e "${GREEN}✓ Disk I/O stats present${NC}"
    checks_passed=$((checks_passed + 1))
else
    echo -e "${RED}✗ Disk I/O stats missing${NC}"
    checks_failed=$((checks_failed + 1))
fi

# Check for CPU stats
if echo "$output" | grep -qE 'cpu.*[0-9]+'; then
    echo -e "${GREEN}✓ CPU stats present${NC}"
    checks_passed=$((checks_passed + 1))
else
    echo -e "${RED}✗ CPU stats missing${NC}"
    checks_failed=$((checks_failed + 1))
fi

echo
echo "Checks passed: $checks_passed"
echo "Checks failed: $checks_failed"

if [ $checks_failed -eq 0 ]; then
    echo -e "${GREEN}✓ pmstat validation passed${NC}"
    exit 0
else
    echo -e "${RED}✗ pmstat validation failed${NC}"
    exit 1
fi
