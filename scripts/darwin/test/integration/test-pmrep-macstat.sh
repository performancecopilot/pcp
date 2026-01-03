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

# Test memory deep-dive view
echo
echo "Testing memory deep-dive :macstat-mem..."
output_mem=$(pmrep -h localhost :macstat-mem -t 1 -s 2 2>&1)
exit_code_mem=$?

if [ $exit_code_mem -ne 0 ]; then
    echo -e "${YELLOW}⚠ pmrep :macstat-mem failed to run${NC}"
    echo "$output_mem"
    # Don't fail the test, just warn
else
    echo "pmrep :macstat-mem output (first 10 lines):"
    echo "---"
    echo "$output_mem" | head -10
    echo "---"
    echo

    checks_mem_passed=0
    checks_mem_failed=0

    # Check for memory metrics
    if echo "$output_mem" | grep -qE '\bphys\b|\bused\b|\bfree\b'; then
        echo -e "${GREEN}✓ Memory breakdown columns present${NC}"
        checks_mem_passed=$((checks_mem_passed + 1))
    else
        echo -e "${YELLOW}⚠ Memory breakdown columns missing${NC}"
        checks_mem_failed=$((checks_mem_failed + 1))
    fi

    # Check for compression metrics
    if echo "$output_mem" | grep -qE '\bcomp\b|\bdeco\b'; then
        echo -e "${GREEN}✓ Compression metrics present${NC}"
        checks_mem_passed=$((checks_mem_passed + 1))
    else
        echo -e "${YELLOW}⚠ Compression metrics missing${NC}"
        checks_mem_failed=$((checks_mem_failed + 1))
    fi

    # Check for cache metrics
    if echo "$output_mem" | grep -qE '\bhit%\b'; then
        echo -e "${GREEN}✓ Cache hit ratio metric present${NC}"
        checks_mem_passed=$((checks_mem_passed + 1))
    else
        echo -e "${YELLOW}⚠ Cache hit ratio metric missing${NC}"
        checks_mem_failed=$((checks_mem_failed + 1))
    fi

    echo
    echo "Memory deep-dive checks passed: $checks_mem_passed"
    echo "Memory deep-dive checks failed: $checks_mem_failed"
fi

# Test disk deep-dive view
echo
echo "Testing disk deep-dive :macstat-dsk..."
output_dsk=$(pmrep -h localhost :macstat-dsk -t 1 -s 2 2>&1)
exit_code_dsk=$?

if [ $exit_code_dsk -ne 0 ]; then
    echo -e "${YELLOW}⚠ pmrep :macstat-dsk failed to run${NC}"
    echo "$output_dsk"
else
    echo "pmrep :macstat-dsk output (first 10 lines):"
    echo "---"
    echo "$output_dsk" | head -10
    echo "---"
    echo

    checks_dsk_passed=0
    checks_dsk_failed=0

    # Check for IOPS metrics
    if echo "$output_dsk" | grep -qE 'r/s|w/s'; then
        echo -e "${GREEN}✓ IOPS metrics present${NC}"
        checks_dsk_passed=$((checks_dsk_passed + 1))
    else
        echo -e "${YELLOW}⚠ IOPS metrics missing${NC}"
        checks_dsk_failed=$((checks_dsk_failed + 1))
    fi

    # Check for throughput metrics
    if echo "$output_dsk" | grep -qE 'rkB/s|wkB/s'; then
        echo -e "${GREEN}✓ Throughput metrics present${NC}"
        checks_dsk_passed=$((checks_dsk_passed + 1))
    else
        echo -e "${YELLOW}⚠ Throughput metrics missing${NC}"
        checks_dsk_failed=$((checks_dsk_failed + 1))
    fi

    # Check for latency metrics
    if echo "$output_dsk" | grep -qE 'rms|wms'; then
        echo -e "${GREEN}✓ Latency metrics present${NC}"
        checks_dsk_passed=$((checks_dsk_passed + 1))
    else
        echo -e "${YELLOW}⚠ Latency metrics missing${NC}"
        checks_dsk_failed=$((checks_dsk_failed + 1))
    fi

    echo
    echo "Disk deep-dive checks passed: $checks_dsk_passed"
    echo "Disk deep-dive checks failed: $checks_dsk_failed"
fi

# Test TCP-focused view
echo
echo "Testing TCP-focused :macstat-tcp..."
output_tcp=$(pmrep -h localhost :macstat-tcp -t 1 -s 2 2>&1)
exit_code_tcp=$?

if [ $exit_code_tcp -ne 0 ]; then
    echo -e "${YELLOW}⚠ pmrep :macstat-tcp failed to run${NC}"
    echo "$output_tcp"
else
    echo "pmrep :macstat-tcp output (first 10 lines):"
    echo "---"
    echo "$output_tcp" | head -10
    echo "---"
    echo

    checks_tcp_passed=0
    checks_tcp_failed=0

    # Check for TCP activity metrics
    if echo "$output_tcp" | grep -qE 'actopn|psvopn'; then
        echo -e "${GREEN}✓ TCP activity metrics present${NC}"
        checks_tcp_passed=$((checks_tcp_passed + 1))
    else
        echo -e "${YELLOW}⚠ TCP activity metrics missing${NC}"
        checks_tcp_failed=$((checks_tcp_failed + 1))
    fi

    # Check for TCP state metrics
    if echo "$output_tcp" | grep -qE 'estab|synst|timew'; then
        echo -e "${GREEN}✓ TCP connection state metrics present${NC}"
        checks_tcp_passed=$((checks_tcp_passed + 1))
    else
        echo -e "${YELLOW}⚠ TCP connection state metrics missing${NC}"
        checks_tcp_failed=$((checks_tcp_failed + 1))
    fi

    # Check for error/retransmission metrics
    if echo "$output_tcp" | grep -qE 'fails|resets|retran'; then
        echo -e "${GREEN}✓ Error/retransmission metrics present${NC}"
        checks_tcp_passed=$((checks_tcp_passed + 1))
    else
        echo -e "${YELLOW}⚠ Error/retransmission metrics missing${NC}"
        checks_tcp_failed=$((checks_tcp_failed + 1))
    fi

    echo
    echo "TCP-focused checks passed: $checks_tcp_passed"
    echo "TCP-focused checks failed: $checks_tcp_failed"
fi

# Test protocol overview view
echo
echo "Testing protocol overview :macstat-proto..."
output_proto=$(pmrep -h localhost :macstat-proto -t 1 -s 2 2>&1)
exit_code_proto=$?

if [ $exit_code_proto -ne 0 ]; then
    echo -e "${YELLOW}⚠ pmrep :macstat-proto failed to run${NC}"
    echo "$output_proto"
else
    echo "pmrep :macstat-proto output (first 10 lines):"
    echo "---"
    echo "$output_proto" | head -10
    echo "---"
    echo

    checks_proto_passed=0
    checks_proto_failed=0

    # Check for UDP metrics
    if echo "$output_proto" | grep -qE 'udpin|udpout'; then
        echo -e "${GREEN}✓ UDP metrics present${NC}"
        checks_proto_passed=$((checks_proto_passed + 1))
    else
        echo -e "${YELLOW}⚠ UDP metrics missing${NC}"
        checks_proto_failed=$((checks_proto_failed + 1))
    fi

    # Check for ICMP metrics
    if echo "$output_proto" | grep -qE 'icmpin|icmpout'; then
        echo -e "${GREEN}✓ ICMP metrics present${NC}"
        checks_proto_passed=$((checks_proto_passed + 1))
    else
        echo -e "${YELLOW}⚠ ICMP metrics missing${NC}"
        checks_proto_failed=$((checks_proto_failed + 1))
    fi

    # Check for socket metrics
    if echo "$output_proto" | grep -qE 'tcpsock|udpsock'; then
        echo -e "${GREEN}✓ Socket metrics present${NC}"
        checks_proto_passed=$((checks_proto_passed + 1))
    else
        echo -e "${YELLOW}⚠ Socket metrics missing${NC}"
        checks_proto_failed=$((checks_proto_failed + 1))
    fi

    echo
    echo "Protocol overview checks passed: $checks_proto_passed"
    echo "Protocol overview checks failed: $checks_proto_failed"
fi

echo
echo "========================================"
echo "Overall Summary"
echo "========================================"
total_passed=$((checks_passed + ${checks_x_passed:-0} + ${checks_mem_passed:-0} + ${checks_dsk_passed:-0} + ${checks_tcp_passed:-0} + ${checks_proto_passed:-0}))
total_failed=$((checks_failed + ${checks_x_failed:-0} + ${checks_mem_failed:-0} + ${checks_dsk_failed:-0} + ${checks_tcp_failed:-0} + ${checks_proto_failed:-0}))

echo "Total checks passed: $total_passed"
echo "Total checks failed: $total_failed"

if [ $checks_failed -eq 0 ]; then
    echo -e "${GREEN}✓ pmrep :macstat validation passed${NC}"
    exit 0
else
    echo -e "${RED}✗ pmrep :macstat validation failed${NC}"
    exit 1
fi
