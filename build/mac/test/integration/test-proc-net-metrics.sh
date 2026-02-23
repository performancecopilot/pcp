#!/bin/bash
#
# Integration test for proc.net metrics (Wave 3c)
# Tests proc.net.tcp_count and proc.net.udp_count
#

set -e

# Color output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

echo "Testing proc.net metrics..."

# Test 1: Verify metrics exist in PMNS
echo "Check 1: Metrics exist in PMNS"
if pminfo proc.net.tcp_count proc.net.udp_count >/dev/null 2>&1; then
    echo -e "${GREEN}✓ Both proc.net metrics exist${NC}"
else
    echo -e "${RED}✗ proc.net metrics not found in PMNS${NC}"
    exit 1
fi

# Test 2: Fetch values for current process (should work for any process)
echo "Check 2: Fetch values"
tcp_count=$(pminfo -f proc.net.tcp_count | grep "value" | head -1 | awk '{print $NF}')
udp_count=$(pminfo -f proc.net.udp_count | grep "value" | head -1 | awk '{print $NF}')

if [[ -n "$tcp_count" && -n "$udp_count" ]]; then
    echo -e "${GREEN}✓ Successfully fetched metric values${NC}"
    echo "  Sample TCP count: $tcp_count"
    echo "  Sample UDP count: $udp_count"
else
    echo -e "${RED}✗ Failed to fetch metric values${NC}"
    exit 1
fi

# Test 3: Verify metric types are numeric (U32)
echo "Check 3: Verify metric types"
tcp_type=$(pminfo -d proc.net.tcp_count | grep "type:" | awk '{print $2}')
udp_type=$(pminfo -d proc.net.udp_count | grep "type:" | awk '{print $2}')

if [[ "$tcp_type" == "32" && "$udp_type" == "32" ]]; then
    echo -e "${GREEN}✓ Metric types are correct (U32)${NC}"
else
    echo -e "${RED}✗ Unexpected metric types: tcp=$tcp_type udp=$udp_type${NC}"
    exit 1
fi

# Test 4: Verify values are non-negative integers
echo "Check 4: Values are valid"
if [[ "$tcp_count" =~ ^[0-9]+$ && "$udp_count" =~ ^[0-9]+$ ]]; then
    echo -e "${GREEN}✓ Values are valid non-negative integers${NC}"
else
    echo -e "${RED}✗ Invalid values: tcp='$tcp_count' udp='$udp_count'${NC}"
    exit 1
fi

# Test 5: Verify metrics have per-process instance domain
echo "Check 5: Instance domain check"
instance_count=$(pminfo -f proc.net.tcp_count | grep -c "inst \[" || true)
if [[ $instance_count -gt 0 ]]; then
    echo -e "${GREEN}✓ Metrics have per-process instances ($instance_count processes)${NC}"
else
    echo -e "${YELLOW}WARNING: No process instances found${NC}"
fi

# Test 6: Sanity check - system should have at least some TCP connections
echo "Check 6: Sanity check - TCP connections exist"
max_tcp=$(pminfo -f proc.net.tcp_count | grep "value" | awk '{print $NF}' | sort -n | tail -1)
if [[ "$max_tcp" -gt 0 ]]; then
    echo -e "${GREEN}✓ System has TCP connections (max per-process: $max_tcp)${NC}"
else
    echo -e "${YELLOW}WARNING: No TCP connections found across all processes${NC}"
fi

echo ""
echo -e "${GREEN}All proc.net metric tests passed!${NC}"
exit 0
