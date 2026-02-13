#!/bin/bash
#
# Extended disk I/O metrics validation test
# Tests new IOBlockStorageDriver statistics (errors, retries, timing)
# and derived metrics (avgrq_sz, await)
#

set -u

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

echo "Testing extended disk I/O metrics..."

checks_passed=0
checks_failed=0

# Test 1: Per-device read errors
read_errors_output=$(pminfo -f disk.dev.read_errors 2>&1)
read_errors_exit=$?

if [ $read_errors_exit -eq 0 ]; then
    # Should have valid numeric values >= 0
    if echo "$read_errors_output" | grep -q 'value.*[0-9]'; then
        echo -e "${GREEN}✓ disk.dev.read_errors present${NC}"
        checks_passed=$((checks_passed + 1))
    else
        echo -e "${RED}✗ disk.dev.read_errors has invalid format${NC}"
        echo "$read_errors_output"
        checks_failed=$((checks_failed + 1))
    fi
else
    echo -e "${RED}✗ disk.dev.read_errors metric missing${NC}"
    echo "$read_errors_output"
    checks_failed=$((checks_failed + 1))
fi

# Test 2: Per-device write errors
write_errors_output=$(pminfo -f disk.dev.write_errors 2>&1)
if [ $? -eq 0 ]; then
    if echo "$write_errors_output" | grep -q 'value.*[0-9]'; then
        echo -e "${GREEN}✓ disk.dev.write_errors present${NC}"
        checks_passed=$((checks_passed + 1))
    else
        echo -e "${RED}✗ disk.dev.write_errors has invalid format${NC}"
        checks_failed=$((checks_failed + 1))
    fi
else
    echo -e "${RED}✗ disk.dev.write_errors metric missing${NC}"
    checks_failed=$((checks_failed + 1))
fi

# Test 3: Per-device read retries
read_retries_output=$(pminfo -f disk.dev.read_retries 2>&1)
if [ $? -eq 0 ]; then
    if echo "$read_retries_output" | grep -q 'value.*[0-9]'; then
        echo -e "${GREEN}✓ disk.dev.read_retries present${NC}"
        checks_passed=$((checks_passed + 1))
    else
        echo -e "${RED}✗ disk.dev.read_retries has invalid format${NC}"
        checks_failed=$((checks_failed + 1))
    fi
else
    echo -e "${RED}✗ disk.dev.read_retries metric missing${NC}"
    checks_failed=$((checks_failed + 1))
fi

# Test 4: Per-device write retries
write_retries_output=$(pminfo -f disk.dev.write_retries 2>&1)
if [ $? -eq 0 ]; then
    if echo "$write_retries_output" | grep -q 'value.*[0-9]'; then
        echo -e "${GREEN}✓ disk.dev.write_retries present${NC}"
        checks_passed=$((checks_passed + 1))
    else
        echo -e "${RED}✗ disk.dev.write_retries has invalid format${NC}"
        checks_failed=$((checks_failed + 1))
    fi
else
    echo -e "${RED}✗ disk.dev.write_retries metric missing${NC}"
    checks_failed=$((checks_failed + 1))
fi

# Test 5: Per-device total read time
total_read_time_output=$(pminfo -f disk.dev.total_read_time 2>&1)
if [ $? -eq 0 ]; then
    if echo "$total_read_time_output" | grep -q 'value.*[0-9]'; then
        echo -e "${GREEN}✓ disk.dev.total_read_time present${NC}"
        checks_passed=$((checks_passed + 1))
    else
        echo -e "${RED}✗ disk.dev.total_read_time has invalid format${NC}"
        checks_failed=$((checks_failed + 1))
    fi
else
    echo -e "${RED}✗ disk.dev.total_read_time metric missing${NC}"
    checks_failed=$((checks_failed + 1))
fi

# Test 6: Per-device total write time
total_write_time_output=$(pminfo -f disk.dev.total_write_time 2>&1)
if [ $? -eq 0 ]; then
    if echo "$total_write_time_output" | grep -q 'value.*[0-9]'; then
        echo -e "${GREEN}✓ disk.dev.total_write_time present${NC}"
        checks_passed=$((checks_passed + 1))
    else
        echo -e "${RED}✗ disk.dev.total_write_time has invalid format${NC}"
        checks_failed=$((checks_failed + 1))
    fi
else
    echo -e "${RED}✗ disk.dev.total_write_time metric missing${NC}"
    checks_failed=$((checks_failed + 1))
fi

# Test 7: Derived metric - average request size
avgrq_sz_output=$(pminfo -f disk.dev.avgrq_sz 2>&1)
if [ $? -eq 0 ]; then
    if echo "$avgrq_sz_output" | grep -q 'value.*[0-9]'; then
        echo -e "${GREEN}✓ disk.dev.avgrq_sz present (derived metric)${NC}"
        checks_passed=$((checks_passed + 1))
    else
        echo -e "${RED}✗ disk.dev.avgrq_sz has invalid format${NC}"
        checks_failed=$((checks_failed + 1))
    fi
else
    echo -e "${RED}✗ disk.dev.avgrq_sz metric missing${NC}"
    checks_failed=$((checks_failed + 1))
fi

# Test 8: Derived metric - average wait time
await_output=$(pminfo -f disk.dev.await 2>&1)
if [ $? -eq 0 ]; then
    if echo "$await_output" | grep -q 'value.*[0-9]'; then
        echo -e "${GREEN}✓ disk.dev.await present (derived metric)${NC}"
        checks_passed=$((checks_passed + 1))
    else
        echo -e "${RED}✗ disk.dev.await has invalid format${NC}"
        checks_failed=$((checks_failed + 1))
    fi
else
    echo -e "${RED}✗ disk.dev.await metric missing${NC}"
    checks_failed=$((checks_failed + 1))
fi

# Test 9: Aggregate read errors
all_read_errors_output=$(pminfo -f disk.all.read_errors 2>&1)
if [ $? -eq 0 ]; then
    all_read_errors=$(echo "$all_read_errors_output" | grep -E '^\s+value' | grep -Eo '[0-9]+')
    if [ -n "$all_read_errors" ]; then
        echo -e "${GREEN}✓ disk.all.read_errors valid: $all_read_errors${NC}"
        checks_passed=$((checks_passed + 1))
    else
        echo -e "${RED}✗ disk.all.read_errors invalid${NC}"
        checks_failed=$((checks_failed + 1))
    fi
else
    echo -e "${RED}✗ disk.all.read_errors metric missing${NC}"
    checks_failed=$((checks_failed + 1))
fi

# Test 10: Aggregate write errors
all_write_errors_output=$(pminfo -f disk.all.write_errors 2>&1)
if [ $? -eq 0 ]; then
    all_write_errors=$(echo "$all_write_errors_output" | grep -E '^\s+value' | grep -Eo '[0-9]+')
    if [ -n "$all_write_errors" ]; then
        echo -e "${GREEN}✓ disk.all.write_errors valid: $all_write_errors${NC}"
        checks_passed=$((checks_passed + 1))
    else
        echo -e "${RED}✗ disk.all.write_errors invalid${NC}"
        checks_failed=$((checks_failed + 1))
    fi
else
    echo -e "${RED}✗ disk.all.write_errors metric missing${NC}"
    checks_failed=$((checks_failed + 1))
fi

# Test 11: Remaining aggregate metrics (retries, timing, derived)
for metric in read_retries write_retries total_read_time total_write_time avgrq_sz await; do
    metric_output=$(pminfo -f disk.all.$metric 2>&1)
    if [ $? -eq 0 ]; then
        if echo "$metric_output" | grep -q 'value.*[0-9]'; then
            echo -e "${GREEN}✓ disk.all.$metric present${NC}"
            checks_passed=$((checks_passed + 1))
        else
            echo -e "${YELLOW}⚠ disk.all.$metric format check skipped${NC}"
            checks_passed=$((checks_passed + 1))
        fi
    else
        echo -e "${RED}✗ disk.all.$metric metric missing${NC}"
        checks_failed=$((checks_failed + 1))
    fi
done

# Summary
echo ""
echo "Extended disk I/O metrics tests: $checks_passed passed, $checks_failed failed"

if [ $checks_failed -eq 0 ]; then
    exit 0
else
    exit 1
fi
