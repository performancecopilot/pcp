#!/bin/bash
#
# APFS metrics validation test
# Tests APFS container and volume metrics via IOKit
#

set -u

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

echo "Testing APFS metrics..."

checks_passed=0
checks_failed=0

# Test 1: Number of APFS containers
ncontainer_output=$(pminfo -f disk.apfs.ncontainer 2>&1)
ncontainer_exit=$?

if [ $ncontainer_exit -eq 0 ]; then
    ncontainer=$(echo "$ncontainer_output" | grep -E '^\s+value' | grep -Eo '[0-9]+')

    if [ -n "$ncontainer" ] && [ "$ncontainer" -ge 0 ]; then
        echo -e "${GREEN}✓ disk.apfs.ncontainer valid: $ncontainer${NC}"
        checks_passed=$((checks_passed + 1))

        # Only test container metrics if we have containers
        if [ "$ncontainer" -gt 0 ]; then
            HAVE_CONTAINERS=1
        else
            HAVE_CONTAINERS=0
            echo -e "${YELLOW}⚠ No APFS containers found, skipping container metric tests${NC}"
        fi
    else
        echo -e "${RED}✗ disk.apfs.ncontainer invalid: $ncontainer${NC}"
        checks_failed=$((checks_failed + 1))
        HAVE_CONTAINERS=0
    fi
else
    echo -e "${RED}✗ disk.apfs.ncontainer metric missing${NC}"
    echo "$ncontainer_output"
    checks_failed=$((checks_failed + 1))
    HAVE_CONTAINERS=0
fi

# Test 2: APFS container metrics (if containers exist)
if [ $HAVE_CONTAINERS -eq 1 ]; then
    # Test container.block_size
    block_size_output=$(pminfo -f disk.apfs.container.block_size 2>&1)
    if [ $? -eq 0 ]; then
        if echo "$block_size_output" | grep -q 'value.*[0-9]'; then
            echo -e "${GREEN}✓ disk.apfs.container.block_size present${NC}"
            checks_passed=$((checks_passed + 1))
        else
            echo -e "${RED}✗ disk.apfs.container.block_size invalid format${NC}"
            checks_failed=$((checks_failed + 1))
        fi
    else
        echo -e "${RED}✗ disk.apfs.container.block_size metric missing${NC}"
        checks_failed=$((checks_failed + 1))
    fi

    # Test container I/O metrics
    for metric in bytes_read bytes_written read_requests write_requests; do
        metric_output=$(pminfo -f disk.apfs.container.$metric 2>&1)
        if [ $? -eq 0 ]; then
            if echo "$metric_output" | grep -q 'value.*[0-9]'; then
                echo -e "${GREEN}✓ disk.apfs.container.$metric present${NC}"
                checks_passed=$((checks_passed + 1))
            else
                echo -e "${RED}✗ disk.apfs.container.$metric invalid format${NC}"
                checks_failed=$((checks_failed + 1))
            fi
        else
            echo -e "${RED}✗ disk.apfs.container.$metric metric missing${NC}"
            checks_failed=$((checks_failed + 1))
        fi
    done

    # Test container transaction metrics
    transactions_output=$(pminfo -f disk.apfs.container.transactions 2>&1)
    if [ $? -eq 0 ]; then
        if echo "$transactions_output" | grep -q 'value.*[0-9]'; then
            echo -e "${GREEN}✓ disk.apfs.container.transactions present${NC}"
            checks_passed=$((checks_passed + 1))
        else
            echo -e "${RED}✗ disk.apfs.container.transactions invalid format${NC}"
            checks_failed=$((checks_failed + 1))
        fi
    else
        echo -e "${RED}✗ disk.apfs.container.transactions metric missing${NC}"
        checks_failed=$((checks_failed + 1))
    fi

    # Test container cache metrics
    for metric in cache_hits cache_evictions; do
        metric_output=$(pminfo -f disk.apfs.container.$metric 2>&1)
        if [ $? -eq 0 ]; then
            if echo "$metric_output" | grep -q 'value.*[0-9]'; then
                echo -e "${GREEN}✓ disk.apfs.container.$metric present${NC}"
                checks_passed=$((checks_passed + 1))
            else
                echo -e "${RED}✗ disk.apfs.container.$metric invalid format${NC}"
                checks_failed=$((checks_failed + 1))
            fi
        else
            echo -e "${RED}✗ disk.apfs.container.$metric metric missing${NC}"
            checks_failed=$((checks_failed + 1))
        fi
    done

    # Test container error metrics
    for metric in read_errors write_errors; do
        metric_output=$(pminfo -f disk.apfs.container.$metric 2>&1)
        if [ $? -eq 0 ]; then
            if echo "$metric_output" | grep -q 'value.*[0-9]'; then
                echo -e "${GREEN}✓ disk.apfs.container.$metric present${NC}"
                checks_passed=$((checks_passed + 1))
            else
                echo -e "${RED}✗ disk.apfs.container.$metric invalid format${NC}"
                checks_failed=$((checks_failed + 1))
            fi
        else
            echo -e "${RED}✗ disk.apfs.container.$metric metric missing${NC}"
            checks_failed=$((checks_failed + 1))
        fi
    done
fi

# Test 3: Number of APFS volumes
nvolume_output=$(pminfo -f disk.apfs.nvolume 2>&1)
nvolume_exit=$?

if [ $nvolume_exit -eq 0 ]; then
    nvolume=$(echo "$nvolume_output" | grep -E '^\s+value' | grep -Eo '[0-9]+')

    if [ -n "$nvolume" ] && [ "$nvolume" -ge 0 ]; then
        echo -e "${GREEN}✓ disk.apfs.nvolume valid: $nvolume${NC}"
        checks_passed=$((checks_passed + 1))

        # Only test volume metrics if we have volumes
        if [ "$nvolume" -gt 0 ]; then
            HAVE_VOLUMES=1
        else
            HAVE_VOLUMES=0
            echo -e "${YELLOW}⚠ No APFS volumes found, skipping volume metric tests${NC}"
        fi
    else
        echo -e "${RED}✗ disk.apfs.nvolume invalid: $nvolume${NC}"
        checks_failed=$((checks_failed + 1))
        HAVE_VOLUMES=0
    fi
else
    echo -e "${RED}✗ disk.apfs.nvolume metric missing${NC}"
    echo "$nvolume_output"
    checks_failed=$((checks_failed + 1))
    HAVE_VOLUMES=0
fi

# Test 4: APFS volume metrics (if volumes exist)
if [ $HAVE_VOLUMES -eq 1 ]; then
    # Test volume.encrypted (should be 0 or 1)
    encrypted_output=$(pminfo -f disk.apfs.volume.encrypted 2>&1)
    if [ $? -eq 0 ]; then
        if echo "$encrypted_output" | grep -qE 'value\s+(0|1)'; then
            echo -e "${GREEN}✓ disk.apfs.volume.encrypted present (boolean)${NC}"
            checks_passed=$((checks_passed + 1))
        else
            echo -e "${RED}✗ disk.apfs.volume.encrypted invalid format (expected 0 or 1)${NC}"
            checks_failed=$((checks_failed + 1))
        fi
    else
        echo -e "${RED}✗ disk.apfs.volume.encrypted metric missing${NC}"
        checks_failed=$((checks_failed + 1))
    fi

    # Test volume.locked (should be 0 or 1)
    locked_output=$(pminfo -f disk.apfs.volume.locked 2>&1)
    if [ $? -eq 0 ]; then
        if echo "$locked_output" | grep -qE 'value\s+(0|1)'; then
            echo -e "${GREEN}✓ disk.apfs.volume.locked present (boolean)${NC}"
            checks_passed=$((checks_passed + 1))
        else
            echo -e "${RED}✗ disk.apfs.volume.locked invalid format (expected 0 or 1)${NC}"
            checks_failed=$((checks_failed + 1))
        fi
    else
        echo -e "${RED}✗ disk.apfs.volume.locked metric missing${NC}"
        checks_failed=$((checks_failed + 1))
    fi
fi

# Summary
echo ""
echo "APFS metrics tests: $checks_passed passed, $checks_failed failed"

if [ $checks_failed -eq 0 ]; then
    exit 0
else
    exit 1
fi
