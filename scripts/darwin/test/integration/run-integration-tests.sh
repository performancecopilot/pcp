#!/bin/bash
#
# Integration test runner for Darwin PMDA
# Tests the installed Darwin PMDA through real PCP tools
#
# Exit codes:
#   0 - All tests passed
#   1 - One or more tests failed
#   2 - Prerequisites not met

set -u

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

# Test results
TESTS_RUN=0
TESTS_PASSED=0
TESTS_FAILED=0

echo "========================================"
echo "Darwin PMDA Integration Test Suite"
echo "========================================"
echo

# Check prerequisites
check_prerequisites() {
    echo "Checking prerequisites..."

    local missing=0

    # Check for pminfo
    if ! command -v pminfo &> /dev/null; then
        echo -e "${RED}✗ pminfo not found${NC}"
        missing=1
    else
        echo -e "${GREEN}✓ pminfo found${NC}"
    fi

    # Check for pmval
    if ! command -v pmval &> /dev/null; then
        echo -e "${RED}✗ pmval not found${NC}"
        missing=1
    else
        echo -e "${GREEN}✓ pmval found${NC}"
    fi

    # Check for pmstat
    if ! command -v pmstat &> /dev/null; then
        echo -e "${RED}✗ pmstat not found${NC}"
        missing=1
    else
        echo -e "${GREEN}✓ pmstat found${NC}"
    fi

    if [ $missing -eq 1 ]; then
        echo -e "${RED}Please install PCP first${NC}"
        return 2
    fi

    # Check if pmcd is running
    if ! pgrep -q pmcd; then
        echo -e "${YELLOW}⚠ pmcd is not running${NC}"
        echo "  Attempting to start pmcd..."
        sudo pmcd start 2>/dev/null || sudo launchctl load /Library/LaunchDaemons/org.pcp.pmcd.plist 2>/dev/null
        sleep 2
        if ! pgrep -q pmcd; then
            echo -e "${RED}✗ Failed to start pmcd${NC}"
            return 2
        fi
    fi
    echo -e "${GREEN}✓ pmcd is running${NC}"

    # Check if darwin PMDA is loaded
    if ! pminfo -f pmcd.agent.status | grep -q darwin; then
        echo -e "${YELLOW}⚠ Darwin PMDA not loaded in pmcd${NC}"
        echo "  Attempting to load darwin PMDA..."
        cd /usr/local/lib/pcp/pmdas/darwin 2>/dev/null || cd /Library/PCP/pmdas/darwin 2>/dev/null
        sudo ./Install < /dev/null
        sleep 1
    fi

    if pminfo darwin.kernel.all.uptime > /dev/null 2>&1; then
        echo -e "${GREEN}✓ Darwin PMDA is loaded and responding${NC}"
    else
        echo -e "${RED}✗ Darwin PMDA is not responding${NC}"
        return 2
    fi

    echo
    return 0
}

# Run a test
run_test() {
    local test_name="$1"
    local test_cmd="$2"

    TESTS_RUN=$((TESTS_RUN + 1))

    echo -n "Testing $test_name... "

    if eval "$test_cmd" > /dev/null 2>&1; then
        echo -e "${GREEN}✓ PASSED${NC}"
        TESTS_PASSED=$((TESTS_PASSED + 1))
        return 0
    else
        echo -e "${RED}✗ FAILED${NC}"
        TESTS_FAILED=$((TESTS_FAILED + 1))
        return 1
    fi
}

# Validate metric value
validate_metric() {
    local metric="$1"
    local validation="$2"

    local value=$(pminfo -f "$metric" 2>/dev/null | grep "value" | awk '{print $2}')

    if [ -z "$value" ]; then
        return 1
    fi

    case "$validation" in
        "exists")
            return 0
            ;;
        "positive")
            [ "$value" -gt 0 ] 2>/dev/null
            ;;
        "non-negative")
            [ "$value" -ge 0 ] 2>/dev/null
            ;;
        *)
            return 1
            ;;
    esac
}

# Main test execution
check_prerequisites || exit $?

echo "Running integration tests..."
echo

# Test 1: Basic metric availability
echo "Test Group: Basic Metrics"
run_test "hinv.ncpu exists" "pminfo -f hinv.ncpu"
run_test "hinv.physmem exists" "pminfo -f hinv.physmem"
run_test "kernel.all.uptime exists" "pminfo -f kernel.all.uptime"
run_test "kernel.all.load exists" "pminfo -f 'kernel.all.load'"
echo

# Test 2: Memory metrics
echo "Test Group: Memory Metrics"
run_test "mem.physmem > 0" "validate_metric mem.physmem positive"
run_test "mem.freemem >= 0" "validate_metric mem.freemem non-negative"
run_test "mem.util.used > 0" "validate_metric mem.util.used positive"
echo

# Test 3: CPU metrics
echo "Test Group: CPU Metrics"
run_test "hinv.ncpu > 0" "validate_metric hinv.ncpu positive"
run_test "kernel.all.cpu.idle >= 0" "validate_metric kernel.all.cpu.idle non-negative"
run_test "kernel.percpu.cpu.user exists" "pminfo 'kernel.percpu.cpu.user'"
echo

# Test 4: pmstat integration
echo "Test Group: pmstat Integration"
run_test "pmstat runs" "timeout 5 pmstat -t 1 -s 2"
run_test "pmstat shows loadavg" "timeout 5 pmstat -t 1 -s 1 | grep -E '[0-9]+\.[0-9]+'"
echo

# Test 5: pmval can fetch values
echo "Test Group: pmval Integration"
run_test "pmval mem.freemem" "timeout 5 pmval -t 1 -s 1 mem.freemem"
run_test "pmval kernel.all.load" "timeout 5 pmval -t 1 -s 1 'kernel.all.load[1]'"
echo

# Test 6: Disk metrics
echo "Test Group: Disk Metrics"
run_test "hinv.ndisk exists" "pminfo -f hinv.ndisk"
run_test "disk.all.total >= 0" "validate_metric disk.all.total non-negative"
echo

# Test 7: Network metrics
echo "Test Group: Network Metrics"
run_test "network.interface.in.bytes exists" "pminfo 'network.interface.in.bytes'"
run_test "network.interface.out.bytes exists" "pminfo 'network.interface.out.bytes'"
echo

# Summary
echo "========================================"
echo "Test Summary"
echo "========================================"
echo "Tests run:    $TESTS_RUN"
echo -e "Tests passed: ${GREEN}$TESTS_PASSED${NC}"
echo -e "Tests failed: ${RED}$TESTS_FAILED${NC}"
echo

if [ $TESTS_FAILED -eq 0 ]; then
    echo -e "${GREEN}✓ All integration tests passed!${NC}"
    exit 0
else
    echo -e "${RED}✗ Some integration tests failed${NC}"
    exit 1
fi
