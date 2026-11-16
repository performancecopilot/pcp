#!/bin/bash
#
# Memory pressure metrics validation test
# Ensures mem.pressure.* report the kernel's own memory pressure verdict
# (kern.memorystatus_vm_pressure_level and kern.memorystatus_level)
#

set -u

RED='\033[0;31m'
GREEN='\033[0;32m'
NC='\033[0m'

echo "Testing memory pressure metrics..."

checks_passed=0
checks_failed=0

metric_number() {
    pminfo -f "$1" 2>/dev/null | grep -E '^\s+value' | grep -Eo '[0-9]+'
}

metric_string() {
    pminfo -f "$1" 2>/dev/null | grep -E '^\s+value' | sed -e 's/.*value "\(.*\)"/\1/'
}

expected_state_for_level() {
    case "$1" in
	1) echo "Normal" ;;
	2) echo "Warning" ;;
	4) echo "Critical" ;;
	*) echo "Unknown" ;;
    esac
}

pass() {
    echo -e "${GREEN}✓ $1${NC}"
    checks_passed=$((checks_passed + 1))
}

fail() {
    echo -e "${RED}✗ $1${NC}"
    checks_failed=$((checks_failed + 1))
}

# Test 1: level matches the kernel's pressure level (1, 2 or 4)
level=$(metric_number mem.pressure.level)
kernel_level=$(sysctl -n kern.memorystatus_vm_pressure_level)
if [ -z "$level" ]; then
    fail "mem.pressure.level missing"
elif [ "$level" -eq "$kernel_level" ]; then
    pass "mem.pressure.level = $level"
else
    fail "mem.pressure.level = $level, kernel reports $kernel_level"
fi

# Test 2: state names the level
state=$(metric_string mem.pressure.state)
expected_state=$(expected_state_for_level "$kernel_level")
if [ -z "$state" ]; then
    fail "mem.pressure.state missing"
elif [ "$state" = "$expected_state" ]; then
    pass "mem.pressure.state = $state"
else
    fail "mem.pressure.state = $state, expected $expected_state"
fi

# Test 3: available percentage tracks the kernel (drifts between samples)
available=$(metric_number mem.pressure.available)
kernel_available=$(sysctl -n kern.memorystatus_level)
if [ -z "$available" ]; then
    fail "mem.pressure.available missing"
elif [ "$available" -gt 100 ]; then
    fail "mem.pressure.available = $available, not a percentage"
elif [ $(( available > kernel_available ? available - kernel_available : kernel_available - available )) -le 5 ]; then
    pass "mem.pressure.available = $available% (kernel $kernel_available%)"
else
    fail "mem.pressure.available = $available%, kernel reports $kernel_available%"
fi

echo
echo "Checks passed: $checks_passed"
echo "Checks failed: $checks_failed"

if [ $checks_failed -eq 0 ]; then
    echo -e "${GREEN}✓ Memory pressure metrics validation passed${NC}"
    exit 0
else
    echo -e "${RED}✗ Memory pressure metrics validation failed${NC}"
    exit 1
fi
