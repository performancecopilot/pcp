#!/bin/bash
#
# IPv6 metrics validation test
# Ensures network.ipv6.* metrics work correctly on macOS
#
# Tests IPv6 protocol statistics via net.inet6.ip6.stats sysctl
#

set -u

RED='\033[0;31m'
GREEN='\033[0;32m'
NC='\033[0m'

echo "Testing IPv6 metrics..."

checks_passed=0
checks_failed=0

# Test 1: inreceives (should be >= 0)
inreceives_output=$(pminfo -f network.ipv6.inreceives 2>&1)
inreceives_exit=$?

if [ $inreceives_exit -eq 0 ]; then
    inreceives=$(echo "$inreceives_output" | grep -E '^\s+value' | grep -Eo '[0-9]+')

    if [ -n "$inreceives" ]; then
        echo -e "${GREEN}✓ inreceives valid: $inreceives${NC}"
        checks_passed=$((checks_passed + 1))
    else
        echo -e "${RED}✗ inreceives invalid: $inreceives${NC}"
        checks_failed=$((checks_failed + 1))
    fi
else
    echo -e "${RED}✗ inreceives metric missing${NC}"
    echo "$inreceives_output"
    checks_failed=$((checks_failed + 1))
fi

# Test 2: outforwarded (should be >= 0)
outforwarded_output=$(pminfo -f network.ipv6.outforwarded 2>&1)
if [ $? -eq 0 ]; then
    outforwarded=$(echo "$outforwarded_output" | grep -E '^\s+value' | grep -Eo '[0-9]+')

    if [ -n "$outforwarded" ]; then
        echo -e "${GREEN}✓ outforwarded valid: $outforwarded${NC}"
        checks_passed=$((checks_passed + 1))
    else
        echo -e "${RED}✗ outforwarded invalid: $outforwarded${NC}"
        checks_failed=$((checks_failed + 1))
    fi
else
    echo -e "${RED}✗ outforwarded metric missing${NC}"
    checks_failed=$((checks_failed + 1))
fi

# Test 3: indiscards (should be >= 0)
indiscards_output=$(pminfo -f network.ipv6.indiscards 2>&1)
if [ $? -eq 0 ]; then
    indiscards=$(echo "$indiscards_output" | grep -E '^\s+value' | grep -Eo '[0-9]+')

    if [ -n "$indiscards" ]; then
        echo -e "${GREEN}✓ indiscards valid: $indiscards${NC}"
        checks_passed=$((checks_passed + 1))
    else
        echo -e "${RED}✗ indiscards invalid: $indiscards${NC}"
        checks_failed=$((checks_failed + 1))
    fi
else
    echo -e "${RED}✗ indiscards metric missing${NC}"
    checks_failed=$((checks_failed + 1))
fi

# Test 4: outdiscards (should be >= 0)
outdiscards_output=$(pminfo -f network.ipv6.outdiscards 2>&1)
if [ $? -eq 0 ]; then
    outdiscards=$(echo "$outdiscards_output" | grep -E '^\s+value' | grep -Eo '[0-9]+')

    if [ -n "$outdiscards" ]; then
        echo -e "${GREEN}✓ outdiscards valid: $outdiscards${NC}"
        checks_passed=$((checks_passed + 1))
    else
        echo -e "${RED}✗ outdiscards invalid: $outdiscards${NC}"
        checks_failed=$((checks_failed + 1))
    fi
else
    echo -e "${RED}✗ outdiscards metric missing${NC}"
    checks_failed=$((checks_failed + 1))
fi

# Test 5: fragcreates (should be >= 0)
fragcreates_output=$(pminfo -f network.ipv6.fragcreates 2>&1)
if [ $? -eq 0 ]; then
    fragcreates=$(echo "$fragcreates_output" | grep -E '^\s+value' | grep -Eo '[0-9]+')

    if [ -n "$fragcreates" ]; then
        echo -e "${GREEN}✓ fragcreates valid: $fragcreates${NC}"
        checks_passed=$((checks_passed + 1))
    else
        echo -e "${RED}✗ fragcreates invalid: $fragcreates${NC}"
        checks_failed=$((checks_failed + 1))
    fi
else
    echo -e "${RED}✗ fragcreates metric missing${NC}"
    checks_failed=$((checks_failed + 1))
fi

# Test 6: reasmoks (should be >= 0)
reasmoks_output=$(pminfo -f network.ipv6.reasmoks 2>&1)
if [ $? -eq 0 ]; then
    reasmoks=$(echo "$reasmoks_output" | grep -E '^\s+value' | grep -Eo '[0-9]+')

    if [ -n "$reasmoks" ]; then
        echo -e "${GREEN}✓ reasmoks valid: $reasmoks${NC}"
        checks_passed=$((checks_passed + 1))
    else
        echo -e "${RED}✗ reasmoks invalid: $reasmoks${NC}"
        checks_failed=$((checks_failed + 1))
    fi
else
    echo -e "${RED}✗ reasmoks metric missing${NC}"
    checks_failed=$((checks_failed + 1))
fi

echo
echo "Checks passed: $checks_passed"
echo "Checks failed: $checks_failed"

if [ $checks_failed -eq 0 ]; then
    echo -e "${GREEN}✓ IPv6 metrics validation passed${NC}"
    exit 0
else
    echo -e "${RED}✗ IPv6 metrics validation failed${NC}"
    exit 1
fi
