#!/bin/bash
#
# Kernel task/thread count validation test
# Ensures kernel.all.nprocs and kernel.all.nthreads report live counts,
# not the kernel's task/thread table limits (kern.num_tasks and
# kern.num_threads are limits despite their names), and that the
# thread limits are exposed as kernel.limits.* ceilings.
#
# Each metric is compared against an independent reference taken
# immediately afterwards; counts drift between samples, so a small
# tolerance is allowed.
#

set -u

RED='\033[0;31m'
GREEN='\033[0;32m'
NC='\033[0m'

echo "Testing kernel task/thread counts..."

checks_passed=0
checks_failed=0

metric_value() {
    pminfo -f "$1" 2>/dev/null | grep -E '^\s+value' | grep -Eo '[0-9]+'
}

reference_process_count() {
    ps -A -o pid= | wc -l | tr -d ' '
}

reference_thread_count() {
    top -l 1 -n 0 | grep -E '^Processes:' | grep -Eo '[0-9]+ threads' | grep -Eo '[0-9]+'
}

# Within 10% of the reference, or 25 absolute, whichever is larger
within_tolerance() {
    local actual=$1 expected=$2
    local diff=$(( actual > expected ? actual - expected : expected - actual ))
    local allowed=$(( expected / 10 > 25 ? expected / 10 : 25 ))
    [ "$diff" -le "$allowed" ]
}

check_count() {
    local metric=$1 reference_fn=$2 limit_sysctl=$3
    local actual expected limit

    actual=$(metric_value "$metric")
    expected=$($reference_fn)
    limit=$(sysctl -n "$limit_sysctl")

    if [ -z "$actual" ]; then
        echo -e "${RED}✗ $metric missing${NC}"
        checks_failed=$((checks_failed + 1))
    elif within_tolerance "$actual" "$expected"; then
        echo -e "${GREEN}✓ $metric = $actual (reference $expected)${NC}"
        checks_passed=$((checks_passed + 1))
    else
        echo -e "${RED}✗ $metric = $actual, reference $expected ($limit_sysctl limit = $limit)${NC}"
        checks_failed=$((checks_failed + 1))
    fi
}

# Test 1: process count tracks ps(1)
check_count kernel.all.nprocs reference_process_count kern.num_tasks

# Test 2: thread count tracks top(1)
check_count kernel.all.nthreads reference_thread_count kern.num_threads

check_limit() {
    local metric=$1 limit_sysctl=$2
    local actual expected

    actual=$(metric_value "$metric")
    expected=$(sysctl -n "$limit_sysctl")

    if [ -z "$actual" ]; then
        echo -e "${RED}✗ $metric missing${NC}"
        checks_failed=$((checks_failed + 1))
    elif [ "$actual" -eq "$expected" ]; then
        echo -e "${GREEN}✓ $metric = $actual${NC}"
        checks_passed=$((checks_passed + 1))
    else
        echo -e "${RED}✗ $metric = $actual, $limit_sysctl = $expected${NC}"
        checks_failed=$((checks_failed + 1))
    fi
}

# Test 3: system-wide thread ceiling
check_limit kernel.limits.maxthreads kern.num_threads

# Test 4: per-process thread ceiling
check_limit kernel.limits.maxtaskthreads kern.num_taskthreads

# Test 5: live thread count sits under its ceiling
nthreads=$(metric_value kernel.all.nthreads)
maxthreads=$(metric_value kernel.limits.maxthreads)
if [ -n "$nthreads" ] && [ -n "$maxthreads" ] && [ "$nthreads" -lt "$maxthreads" ]; then
    echo -e "${GREEN}✓ kernel.all.nthreads $nthreads < kernel.limits.maxthreads $maxthreads${NC}"
    checks_passed=$((checks_passed + 1))
else
    echo -e "${RED}✗ kernel.all.nthreads ${nthreads:-missing} not under kernel.limits.maxthreads ${maxthreads:-missing}${NC}"
    checks_failed=$((checks_failed + 1))
fi

echo
echo "Checks passed: $checks_passed"
echo "Checks failed: $checks_failed"

if [ $checks_failed -eq 0 ]; then
    echo -e "${GREEN}✓ Kernel task/thread count validation passed${NC}"
    exit 0
else
    echo -e "${RED}✗ Kernel task/thread count validation failed${NC}"
    exit 1
fi
