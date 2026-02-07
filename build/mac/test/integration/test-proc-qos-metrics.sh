#!/bin/bash
#
# Process QoS CPU time metrics validation test
# Ensures proc.cpu.qos.* metrics work correctly on macOS
#
# Tests per-process QoS-tier CPU time accounting via RUSAGE_INFO_V4
#

set -u

RED='\033[0;31m'
GREEN='\033[0;32m'
NC='\033[0m'

echo "Testing Process QoS CPU time metrics..."

checks_passed=0
checks_failed=0

# Get the current shell PID for testing
SHELL_PID=$$

# Test 1: proc.cpu.qos.default
default_output=$(pminfo -f proc.cpu.qos.default 2>&1)
default_exit=$?

if [ $default_exit -eq 0 ]; then
    # Check that metric exists and has per-process instances
    default_instances=$(echo "$default_output" | grep -c "inst \[")

    if [ "$default_instances" -gt 0 ]; then
        echo -e "${GREEN}✓ proc.cpu.qos.default valid ($default_instances process instances)${NC}"
        checks_passed=$((checks_passed + 1))
    else
        echo -e "${RED}✗ proc.cpu.qos.default has no instances${NC}"
        checks_failed=$((checks_failed + 1))
    fi
else
    echo -e "${RED}✗ proc.cpu.qos.default metric missing${NC}"
    echo "$default_output"
    checks_failed=$((checks_failed + 1))
fi

# Test 2: proc.cpu.qos.maintenance
maintenance_output=$(pminfo -f proc.cpu.qos.maintenance 2>&1)
if [ $? -eq 0 ]; then
    maintenance_instances=$(echo "$maintenance_output" | grep -c "inst \[")

    if [ "$maintenance_instances" -gt 0 ]; then
        echo -e "${GREEN}✓ proc.cpu.qos.maintenance valid ($maintenance_instances instances)${NC}"
        checks_passed=$((checks_passed + 1))
    else
        echo -e "${RED}✗ proc.cpu.qos.maintenance has no instances${NC}"
        checks_failed=$((checks_failed + 1))
    fi
else
    echo -e "${RED}✗ proc.cpu.qos.maintenance metric missing${NC}"
    checks_failed=$((checks_failed + 1))
fi

# Test 3: proc.cpu.qos.background
background_output=$(pminfo -f proc.cpu.qos.background 2>&1)
if [ $? -eq 0 ]; then
    background_instances=$(echo "$background_output" | grep -c "inst \[")

    if [ "$background_instances" -gt 0 ]; then
        echo -e "${GREEN}✓ proc.cpu.qos.background valid ($background_instances instances)${NC}"
        checks_passed=$((checks_passed + 1))
    else
        echo -e "${RED}✗ proc.cpu.qos.background has no instances${NC}"
        checks_failed=$((checks_failed + 1))
    fi
else
    echo -e "${RED}✗ proc.cpu.qos.background metric missing${NC}"
    checks_failed=$((checks_failed + 1))
fi

# Test 4: proc.cpu.qos.utility
utility_output=$(pminfo -f proc.cpu.qos.utility 2>&1)
if [ $? -eq 0 ]; then
    utility_instances=$(echo "$utility_output" | grep -c "inst \[")

    if [ "$utility_instances" -gt 0 ]; then
        echo -e "${GREEN}✓ proc.cpu.qos.utility valid ($utility_instances instances)${NC}"
        checks_passed=$((checks_passed + 1))
    else
        echo -e "${RED}✗ proc.cpu.qos.utility has no instances${NC}"
        checks_failed=$((checks_failed + 1))
    fi
else
    echo -e "${RED}✗ proc.cpu.qos.utility metric missing${NC}"
    checks_failed=$((checks_failed + 1))
fi

# Test 5: proc.cpu.qos.legacy
legacy_output=$(pminfo -f proc.cpu.qos.legacy 2>&1)
if [ $? -eq 0 ]; then
    legacy_instances=$(echo "$legacy_output" | grep -c "inst \[")

    if [ "$legacy_instances" -gt 0 ]; then
        echo -e "${GREEN}✓ proc.cpu.qos.legacy valid ($legacy_instances instances)${NC}"
        checks_passed=$((checks_passed + 1))
    else
        echo -e "${RED}✗ proc.cpu.qos.legacy has no instances${NC}"
        checks_failed=$((checks_failed + 1))
    fi
else
    echo -e "${RED}✗ proc.cpu.qos.legacy metric missing${NC}"
    checks_failed=$((checks_failed + 1))
fi

# Test 6: proc.cpu.qos.user_initiated
user_initiated_output=$(pminfo -f proc.cpu.qos.user_initiated 2>&1)
if [ $? -eq 0 ]; then
    user_initiated_instances=$(echo "$user_initiated_output" | grep -c "inst \[")

    if [ "$user_initiated_instances" -gt 0 ]; then
        echo -e "${GREEN}✓ proc.cpu.qos.user_initiated valid ($user_initiated_instances instances)${NC}"
        checks_passed=$((checks_passed + 1))
    else
        echo -e "${RED}✗ proc.cpu.qos.user_initiated has no instances${NC}"
        checks_failed=$((checks_failed + 1))
    fi
else
    echo -e "${RED}✗ proc.cpu.qos.user_initiated metric missing${NC}"
    checks_failed=$((checks_failed + 1))
fi

# Test 7: proc.cpu.qos.user_interactive
user_interactive_output=$(pminfo -f proc.cpu.qos.user_interactive 2>&1)
if [ $? -eq 0 ]; then
    user_interactive_instances=$(echo "$user_interactive_output" | grep -c "inst \[")

    if [ "$user_interactive_instances" -gt 0 ]; then
        echo -e "${GREEN}✓ proc.cpu.qos.user_interactive valid ($user_interactive_instances instances)${NC}"
        checks_passed=$((checks_passed + 1))
    else
        echo -e "${RED}✗ proc.cpu.qos.user_interactive has no instances${NC}"
        checks_failed=$((checks_failed + 1))
    fi
else
    echo -e "${RED}✗ proc.cpu.qos.user_interactive metric missing${NC}"
    checks_failed=$((checks_failed + 1))
fi

echo
echo "Checks passed: $checks_passed"
echo "Checks failed: $checks_failed"

if [ $checks_failed -eq 0 ]; then
    echo -e "${GREEN}✓ Process QoS metrics validation passed${NC}"
    exit 0
else
    echo -e "${RED}✗ Process QoS metrics validation failed${NC}"
    exit 1
fi
