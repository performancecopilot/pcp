#!/bin/bash
#
# Detailed pmrep :macstat validation test
# Ensures pmrep with macstat config shows macOS-specific metrics correctly
#

set -u

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

echo "Testing pmrep :macstat with Darwin PMDA..."

# Run pmrep :macstat for 2 samples (config should be installed)
# Use -h localhost to force TCP connection (more reliable in CI environments)
echo "Testing basic :macstat..."
output=$(pmrep -h localhost :macstat -t 1 -s 2 2>&1)
exit_code=$?

if [ $exit_code -ne 0 ]; then
    echo -e "${RED}✗ pmrep :macstat failed to run${NC}"
    echo "$output"
    exit 1
fi

echo "pmrep :macstat output:"
echo "---"
echo "$output"
echo "---"
echo

# Validate output contains expected columns
checks_passed=0
checks_failed=0

# Check for load average
if echo "$output" | grep -qE 'load avg|1 minute'; then
    echo -e "${GREEN}✓ Load average column present${NC}"
    checks_passed=$((checks_passed + 1))
else
    echo -e "${RED}✗ Load average column missing${NC}"
    checks_failed=$((checks_failed + 1))
fi

# Check for memory columns (macOS-specific)
if echo "$output" | grep -qE 'wired|active|free|cmpr'; then
    echo -e "${GREEN}✓ Memory columns present (macOS model)${NC}"
    checks_passed=$((checks_passed + 1))
else
    echo -e "${RED}✗ Memory columns missing${NC}"
    checks_failed=$((checks_failed + 1))
fi

# Check for paging activity
if echo "$output" | grep -qE '\bpi\b.*\bpo\b'; then
    echo -e "${GREEN}✓ Paging activity columns present${NC}"
    checks_passed=$((checks_passed + 1))
else
    echo -e "${RED}✗ Paging activity columns missing${NC}"
    checks_failed=$((checks_failed + 1))
fi

# Check for disk I/O
if echo "$output" | grep -qE '\bread\b.*\bwrite\b'; then
    echo -e "${GREEN}✓ Disk I/O columns present${NC}"
    checks_passed=$((checks_passed + 1))
else
    echo -e "${RED}✗ Disk I/O columns missing${NC}"
    checks_failed=$((checks_failed + 1))
fi

# Check for CPU stats
if echo "$output" | grep -qE '\bus\b.*\bsy\b.*\bid\b'; then
    echo -e "${GREEN}✓ CPU stats columns present${NC}"
    checks_passed=$((checks_passed + 1))
else
    echo -e "${RED}✗ CPU stats columns missing${NC}"
    checks_failed=$((checks_failed + 1))
fi

# Check that we have numeric values (not all question marks)
if echo "$output" | grep -qE '[0-9]+'; then
    echo -e "${GREEN}✓ Numeric values present${NC}"
    checks_passed=$((checks_passed + 1))
else
    echo -e "${RED}✗ No numeric values found${NC}"
    checks_failed=$((checks_failed + 1))
fi

echo
echo "Basic macstat checks passed: $checks_passed"
echo "Basic macstat checks failed: $checks_failed"

# Test extended macstat-x
echo
echo "Testing extended :macstat-x..."
output_x=$(pmrep -h localhost :macstat-x -t 1 -s 2 2>&1)
exit_code_x=$?

if [ $exit_code_x -ne 0 ]; then
    echo -e "${YELLOW}⚠ pmrep :macstat-x failed to run${NC}"
    echo "$output_x"
    # Don't fail the test, just warn
else
    echo "pmrep :macstat-x output:"
    echo "---"
    echo "$output_x"
    echo "---"
    echo

    # Check for extended features
    checks_x_passed=0
    checks_x_failed=0

    # Check for inactive memory (extended only)
    if echo "$output_x" | grep -qE '\binact\b'; then
        echo -e "${GREEN}✓ Inactive memory column present${NC}"
        checks_x_passed=$((checks_x_passed + 1))
    else
        echo -e "${YELLOW}⚠ Inactive memory column missing${NC}"
        checks_x_failed=$((checks_x_failed + 1))
    fi

    # Check for compressor activity
    if echo "$output_x" | grep -qE '\bcomp\b.*\bdeco\b'; then
        echo -e "${GREEN}✓ Compressor activity columns present${NC}"
        checks_x_passed=$((checks_x_passed + 1))
    else
        echo -e "${YELLOW}⚠ Compressor activity columns missing${NC}"
        checks_x_failed=$((checks_x_failed + 1))
    fi

    # Check for process/thread counts
    if echo "$output_x" | grep -qE '\bproc\b.*\bthrd\b'; then
        echo -e "${GREEN}✓ Process/thread count columns present${NC}"
        checks_x_passed=$((checks_x_passed + 1))
    else
        echo -e "${YELLOW}⚠ Process/thread count columns missing${NC}"
        checks_x_failed=$((checks_x_failed + 1))
    fi

    echo
    echo "Extended macstat-x checks passed: $checks_x_passed"
    echo "Extended macstat-x checks failed: $checks_x_failed"
fi

echo
echo "========================================"
echo "Overall Summary"
echo "========================================"
total_passed=$((checks_passed + ${checks_x_passed:-0}))
total_failed=$((checks_failed + ${checks_x_failed:-0}))

echo "Total checks passed: $total_passed"
echo "Total checks failed: $total_failed"

if [ $checks_failed -eq 0 ]; then
    echo -e "${GREEN}✓ pmrep :macstat validation passed${NC}"
    exit 0
else
    echo -e "${RED}✗ pmrep :macstat validation failed${NC}"
    exit 1
fi
