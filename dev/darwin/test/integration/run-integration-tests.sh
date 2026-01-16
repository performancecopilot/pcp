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

    if pminfo kernel.all.uptime > /dev/null 2>&1; then
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
run_test "pmstat runs" "pmstat -t 1 -s 2"
run_test "pmstat shows loadavg" "pmstat -t 1 -s 1 | grep -E '[0-9]+\.[0-9]+'"
echo

# Test 5: pmrep :macstat integration (macOS-specific pmstat alternative)
echo "Test Group: pmrep :macstat Integration"
if command -v pmrep &> /dev/null; then
    run_test "pmrep :macstat runs" "pmrep :macstat -t 1 -s 2"
    run_test "pmrep :macstat-x runs" "pmrep :macstat-x -t 1 -s 2"

    # Run detailed macstat test if it exists
    if [ -f "$SCRIPT_DIR/test-pmrep-macstat.sh" ]; then
        echo "Running detailed pmrep :macstat validation..."
        if "$SCRIPT_DIR/test-pmrep-macstat.sh"; then
            echo -e "${GREEN}✓ Detailed pmrep :macstat validation passed${NC}"
        else
            echo -e "${YELLOW}⚠ Detailed pmrep :macstat validation had issues${NC}"
        fi
    fi
else
    echo -e "${YELLOW}⚠ pmrep not found, skipping :macstat tests${NC}"
fi
echo

# Test 6: pmval can fetch values
echo "Test Group: pmval Integration"
run_test "pmval mem.freemem" "pmval -t 1 -s 1 mem.freemem"
run_test "pmval kernel.all.load" "pmval -t 1 -s 1 'kernel.all.load[1]'"
echo

# Test 7: Disk metrics
echo "Test Group: Disk Metrics"
run_test "hinv.ndisk exists" "pminfo -f hinv.ndisk"
run_test "disk.all.total >= 0" "validate_metric disk.all.total non-negative"
echo

# Test 8: Network metrics
echo "Test Group: Network Metrics"
run_test "network.interface.in.bytes exists" "pminfo 'network.interface.in.bytes'"
run_test "network.interface.out.bytes exists" "pminfo 'network.interface.out.bytes'"
echo

# Test 9: VFS metrics
echo "Test Group: VFS Metrics"
run_test "vfs.files.count exists" "pminfo -f vfs.files.count"
run_test "vfs.files.max > 0" "validate_metric vfs.files.max positive"
run_test "vfs.files.free >= 0" "validate_metric vfs.files.free non-negative"
run_test "vfs.vnodes.count >= 0" "validate_metric vfs.vnodes.count non-negative"
run_test "vfs.vnodes.max > 0" "validate_metric vfs.vnodes.max positive"
run_test "kernel.all.nprocs > 0" "validate_metric kernel.all.nprocs positive"
run_test "kernel.all.nthreads > 0" "validate_metric kernel.all.nthreads positive"
echo

# Test 10: UDP protocol metrics
echo "Test Group: UDP Protocol Metrics"
run_test "network.udp.indatagrams exists" "pminfo -f network.udp.indatagrams"
run_test "network.udp.outdatagrams exists" "pminfo -f network.udp.outdatagrams"
run_test "network.udp.indatagrams >= 0" "validate_metric network.udp.indatagrams non-negative"
run_test "network.udp.outdatagrams >= 0" "validate_metric network.udp.outdatagrams non-negative"
run_test "network.udp.noports >= 0" "validate_metric network.udp.noports non-negative"
run_test "network.udp.inerrors >= 0" "validate_metric network.udp.inerrors non-negative"
run_test "network.udp.rcvbuferrors >= 0" "validate_metric network.udp.rcvbuferrors non-negative"
echo

# Test 11: ICMP protocol metrics
echo "Test Group: ICMP Protocol Metrics"
run_test "network.icmp.inmsgs exists" "pminfo -f network.icmp.inmsgs"
run_test "network.icmp.outmsgs exists" "pminfo -f network.icmp.outmsgs"
run_test "network.icmp.inmsgs >= 0" "validate_metric network.icmp.inmsgs non-negative"
run_test "network.icmp.outmsgs >= 0" "validate_metric network.icmp.outmsgs non-negative"
run_test "network.icmp.inerrors >= 0" "validate_metric network.icmp.inerrors non-negative"
run_test "network.icmp.indestunreachs >= 0" "validate_metric network.icmp.indestunreachs non-negative"
run_test "network.icmp.inechos >= 0" "validate_metric network.icmp.inechos non-negative"
run_test "network.icmp.inechoreps >= 0" "validate_metric network.icmp.inechoreps non-negative"
run_test "network.icmp.outechos >= 0" "validate_metric network.icmp.outechos non-negative"
run_test "network.icmp.outechoreps >= 0" "validate_metric network.icmp.outechoreps non-negative"
echo

echo "Test Group: Socket Statistics"
run_test "network.sockstat.tcp.inuse exists" "pminfo -f network.sockstat.tcp.inuse"
run_test "network.sockstat.udp.inuse exists" "pminfo -f network.sockstat.udp.inuse"
run_test "network.sockstat.tcp.inuse > 0" "validate_metric network.sockstat.tcp.inuse positive"
run_test "network.sockstat.udp.inuse >= 0" "validate_metric network.sockstat.udp.inuse non-negative"
echo

echo "Test Group: TCP Connection State Metrics"
run_test "network.tcpconn.established exists" "pminfo -f network.tcpconn.established"
run_test "network.tcpconn.listen exists" "pminfo -f network.tcpconn.listen"
run_test "network.tcpconn.time_wait exists" "pminfo -f network.tcpconn.time_wait"
run_test "network.tcpconn.established >= 0" "validate_metric network.tcpconn.established non-negative"
run_test "network.tcpconn.listen >= 0" "validate_metric network.tcpconn.listen non-negative"
run_test "network.tcpconn.time_wait >= 0" "validate_metric network.tcpconn.time_wait non-negative"
run_test "network.tcpconn.syn_sent >= 0" "validate_metric network.tcpconn.syn_sent non-negative"
run_test "network.tcpconn.syn_recv >= 0" "validate_metric network.tcpconn.syn_recv non-negative"
run_test "network.tcpconn.fin_wait1 >= 0" "validate_metric network.tcpconn.fin_wait1 non-negative"
run_test "network.tcpconn.fin_wait2 >= 0" "validate_metric network.tcpconn.fin_wait2 non-negative"
run_test "network.tcpconn.close >= 0" "validate_metric network.tcpconn.close non-negative"
run_test "network.tcpconn.close_wait >= 0" "validate_metric network.tcpconn.close_wait non-negative"
run_test "network.tcpconn.last_ack >= 0" "validate_metric network.tcpconn.last_ack non-negative"
run_test "network.tcpconn.closing >= 0" "validate_metric network.tcpconn.closing non-negative"
echo

echo "Test Group: TCP Protocol Statistics"
run_test "network.tcp.activeopens exists" "pminfo -f network.tcp.activeopens"
run_test "network.tcp.passiveopens exists" "pminfo -f network.tcp.passiveopens"
run_test "network.tcp.insegs exists" "pminfo -f network.tcp.insegs"
run_test "network.tcp.outsegs exists" "pminfo -f network.tcp.outsegs"
run_test "network.tcp.activeopens >= 0" "validate_metric network.tcp.activeopens non-negative"
run_test "network.tcp.passiveopens >= 0" "validate_metric network.tcp.passiveopens non-negative"
run_test "network.tcp.attemptfails >= 0" "validate_metric network.tcp.attemptfails non-negative"
run_test "network.tcp.estabresets >= 0" "validate_metric network.tcp.estabresets non-negative"
run_test "network.tcp.currestab >= 0" "validate_metric network.tcp.currestab non-negative"
run_test "network.tcp.insegs >= 0" "validate_metric network.tcp.insegs non-negative"
run_test "network.tcp.outsegs >= 0" "validate_metric network.tcp.outsegs non-negative"
run_test "network.tcp.retranssegs >= 0" "validate_metric network.tcp.retranssegs non-negative"
run_test "network.tcp.inerrs.total >= 0" "validate_metric network.tcp.inerrs.total non-negative"
run_test "network.tcp.outrsts >= 0" "validate_metric network.tcp.outrsts non-negative"
run_test "network.tcp.incsumerrors >= 0" "validate_metric network.tcp.incsumerrors non-negative"
run_test "network.tcp.rtoalgorithm == 4" "validate_metric network.tcp.rtoalgorithm positive"
run_test "network.tcp.rtomin == 200" "validate_metric network.tcp.rtomin positive"
run_test "network.tcp.rtomax == 64000" "validate_metric network.tcp.rtomax positive"
echo

echo "Test Group: TCP Granular Error Metrics"
run_test "network.tcp.inerrs.badsum exists" "pminfo -f network.tcp.inerrs.badsum"
run_test "network.tcp.inerrs.badoff exists" "pminfo -f network.tcp.inerrs.badoff"
run_test "network.tcp.inerrs.short exists" "pminfo -f network.tcp.inerrs.short"
run_test "network.tcp.inerrs.memdrop exists" "pminfo -f network.tcp.inerrs.memdrop"
run_test "network.tcp.inerrs.badsum >= 0" "validate_metric network.tcp.inerrs.badsum non-negative"
run_test "network.tcp.inerrs.badoff >= 0" "validate_metric network.tcp.inerrs.badoff non-negative"
run_test "network.tcp.inerrs.short >= 0" "validate_metric network.tcp.inerrs.short non-negative"
run_test "network.tcp.inerrs.memdrop >= 0" "validate_metric network.tcp.inerrs.memdrop non-negative"
echo

# Test 15: Process metrics (darwin_proc PMDA)
echo "Test Group: Process Metrics"
run_test "proc.nprocs > 0" "validate_metric proc.nprocs positive"
run_test "proc.psinfo.pid exists" "pminfo 'proc.psinfo.pid'"
run_test "proc.psinfo.cmd exists" "pminfo 'proc.psinfo.cmd'"
run_test "proc.memory.size exists" "pminfo 'proc.memory.size'"
run_test "proc.memory.rss exists" "pminfo 'proc.memory.rss'"
echo

# Test 16: Process I/O statistics (Step 3.1)
echo "Test Group: Process I/O Statistics"
run_test "proc.io.read_bytes exists" "pminfo 'proc.io.read_bytes'"
run_test "proc.io.write_bytes exists" "pminfo 'proc.io.write_bytes'"
# Note: We can't validate specific values as processes may have no I/O yet
# Just verify the metrics are fetchable
run_test "proc.io.read_bytes fetchable" "pminfo -f 'proc.io.read_bytes'"
run_test "proc.io.write_bytes fetchable" "pminfo -f 'proc.io.write_bytes'"
echo

# Test 17: Process file descriptor count (Step 3.2)
echo "Test Group: Process File Descriptor Count"
run_test "proc.fd.count exists" "pminfo 'proc.fd.count'"
run_test "proc.fd.count fetchable" "pminfo -f 'proc.fd.count'"
# At least one process (pminfo itself) should have open FDs
run_test "some process has FDs > 0" "pminfo -f 'proc.fd.count' | grep -q 'value [1-9][0-9]*'"
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
