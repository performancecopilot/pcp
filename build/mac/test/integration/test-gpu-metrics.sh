#!/bin/bash
#
# GPU metrics validation test
# Ensures darwin.gpu.* metrics work correctly on macOS
#

set -u

RED='\033[0;31m'
GREEN='\033[0;32m'
NC='\033[0m'

echo "Testing GPU metrics..."

checks_passed=0
checks_failed=0

# Test 1: GPU count metric exists and returns valid value
ngpu_output=$(pminfo -f hinv.ngpu 2>&1)
ngpu_exit=$?

if [ $ngpu_exit -eq 0 ]; then
    # Extract value (format: "value N" or just "N")
    ngpu_value=$(echo "$ngpu_output" | grep -Eo 'value [0-9]+' | awk '{print $2}')
    if [ -z "$ngpu_value" ]; then
        ngpu_value=$(echo "$ngpu_output" | grep -Eo '\s[0-9]+$' | tr -d ' ')
    fi

    if [ -n "$ngpu_value" ] && [ "$ngpu_value" -ge 0 ] && [ "$ngpu_value" -le 4 ]; then
        echo -e "${GREEN}✓ GPU count metric present (darwin.hinv.ngpu = $ngpu_value)${NC}"
        checks_passed=$((checks_passed + 1))
    else
        echo -e "${RED}✗ GPU count value invalid: $ngpu_value${NC}"
        checks_failed=$((checks_failed + 1))
    fi
else
    echo -e "${RED}✗ GPU count metric missing${NC}"
    echo "$ngpu_output"
    checks_failed=$((checks_failed + 1))
fi

# Test 2: GPU utilization metric exists
if pminfo gpu.util >/dev/null 2>&1; then
    echo -e "${GREEN}✓ GPU utilization metric exists${NC}"
    checks_passed=$((checks_passed + 1))
else
    echo -e "${RED}✗ GPU utilization metric missing${NC}"
    checks_failed=$((checks_failed + 1))
fi

# Test 3: GPU memory.used metric exists
if pminfo gpu.memory.used >/dev/null 2>&1; then
    echo -e "${GREEN}✓ GPU memory.used metric exists${NC}"
    checks_passed=$((checks_passed + 1))
else
    echo -e "${RED}✗ GPU memory.used metric missing${NC}"
    checks_failed=$((checks_failed + 1))
fi

# Test 4: GPU memory.free metric exists
if pminfo gpu.memory.free >/dev/null 2>&1; then
    echo -e "${GREEN}✓ GPU memory.free metric exists${NC}"
    checks_passed=$((checks_passed + 1))
else
    echo -e "${RED}✗ GPU memory.free metric missing${NC}"
    checks_failed=$((checks_failed + 1))
fi

# Test 5: Instance domain validation (only if GPUs present)
if [ -n "${ngpu_value:-}" ] && [ "$ngpu_value" -gt 0 ]; then
    util_output=$(pminfo -f gpu.util 2>&1)
    if echo "$util_output" | grep -q "gpu0"; then
        echo -e "${GREEN}✓ Instance domain shows gpu0${NC}"
        checks_passed=$((checks_passed + 1))
    else
        echo -e "${RED}✗ Instance domain missing gpu0${NC}"
        echo "$util_output"
        checks_failed=$((checks_failed + 1))
    fi

    # Test 6: Utilization value in valid range (0-100)
    util_value=$(echo "$util_output" | grep -A1 'inst \[0' | grep value | grep -Eo '[0-9]+')
    if [ -n "$util_value" ] && [ "$util_value" -ge 0 ] && [ "$util_value" -le 100 ]; then
        echo -e "${GREEN}✓ Utilization value in valid range (0-100): $util_value${NC}"
        checks_passed=$((checks_passed + 1))
    else
        echo -e "${RED}✗ Utilization value invalid: $util_value${NC}"
        checks_failed=$((checks_failed + 1))
    fi

    # Test 7: Memory values are non-negative
    mem_used_output=$(pminfo -f gpu.memory.used 2>&1)
    mem_used_value=$(echo "$mem_used_output" | grep -A1 'inst \[0' | grep value | grep -Eo '[0-9]+')

    mem_free_output=$(pminfo -f gpu.memory.free 2>&1)
    mem_free_value=$(echo "$mem_free_output" | grep -A1 'inst \[0' | grep value | grep -Eo '[0-9]+')

    if [ -n "$mem_used_value" ] && [ "$mem_used_value" -ge 0 ] && [ -n "$mem_free_value" ] && [ "$mem_free_value" -ge 0 ]; then
        echo -e "${GREEN}✓ Memory values are non-negative (used=$mem_used_value, free=$mem_free_value)${NC}"
        checks_passed=$((checks_passed + 1))
    else
        echo -e "${RED}✗ Memory values invalid${NC}"
        checks_failed=$((checks_failed + 1))
    fi
else
    echo "Skipping instance domain tests (no GPUs present)"
fi

echo
echo "Checks passed: $checks_passed"
echo "Checks failed: $checks_failed"

if [ $checks_failed -eq 0 ]; then
    echo -e "${GREEN}✓ GPU metrics validation passed${NC}"
    exit 0
else
    echo -e "${RED}✗ GPU metrics validation failed${NC}"
    exit 1
fi
