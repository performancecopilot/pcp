#!/bin/bash
#
# IPC metrics validation test
# Ensures darwin.ipc.* metrics work correctly on macOS
#
# Tests kernel IPC resource limits and usage via sysctl
#

set -u

RED='\033[0;31m'
GREEN='\033[0;32m'
NC='\033[0m'

echo "Testing IPC metrics..."

checks_passed=0
checks_failed=0

# Test 1: mbuf clusters (should always be > 0)
mbuf_output=$(pminfo -f ipc.mbuf.clusters 2>&1)
mbuf_exit=$?

if [ $mbuf_exit -eq 0 ]; then
    mbuf_clusters=$(echo "$mbuf_output" | grep -E '^\s+value' | grep -Eo '[0-9]+')

    if [ -n "$mbuf_clusters" ] && [ "$mbuf_clusters" -gt 0 ]; then
        echo -e "${GREEN}✓ mbuf clusters valid: $mbuf_clusters${NC}"
        checks_passed=$((checks_passed + 1))
    else
        echo -e "${RED}✗ mbuf clusters invalid: $mbuf_clusters (expected > 0)${NC}"
        checks_failed=$((checks_failed + 1))
    fi
else
    echo -e "${RED}✗ mbuf clusters metric missing${NC}"
    echo "$mbuf_output"
    checks_failed=$((checks_failed + 1))
fi

# Test 2: max socket buffer size (should be > 0, typically in MB range)
maxsockbuf_output=$(pminfo -f ipc.maxsockbuf 2>&1)
if [ $? -eq 0 ]; then
    maxsockbuf=$(echo "$maxsockbuf_output" | grep -E '^\s+value' | grep -Eo '[0-9]+')

    if [ -n "$maxsockbuf" ] && [ "$maxsockbuf" -gt 0 ]; then
        # Convert to human-readable (KB)
        maxsockbuf_kb=$((maxsockbuf / 1024))
        echo -e "${GREEN}✓ max socket buffer valid: $maxsockbuf bytes (${maxsockbuf_kb}KB)${NC}"
        checks_passed=$((checks_passed + 1))
    else
        echo -e "${RED}✗ max socket buffer invalid: $maxsockbuf${NC}"
        checks_failed=$((checks_failed + 1))
    fi
else
    echo -e "${RED}✗ max socket buffer metric missing${NC}"
    checks_failed=$((checks_failed + 1))
fi

# Test 3: somaxconn (listen backlog limit, should be > 0)
somaxconn_output=$(pminfo -f ipc.somaxconn 2>&1)
if [ $? -eq 0 ]; then
    somaxconn=$(echo "$somaxconn_output" | grep -E '^\s+value' | grep -Eo '[0-9]+')

    if [ -n "$somaxconn" ] && [ "$somaxconn" -gt 0 ]; then
        echo -e "${GREEN}✓ somaxconn valid: $somaxconn${NC}"
        checks_passed=$((checks_passed + 1))
    else
        echo -e "${RED}✗ somaxconn invalid: $somaxconn${NC}"
        checks_failed=$((checks_failed + 1))
    fi
else
    echo -e "${RED}✗ somaxconn metric missing${NC}"
    checks_failed=$((checks_failed + 1))
fi

# Test 4: defunct socket count (should be >= 0)
defunct_output=$(pminfo -f ipc.socket.defunct 2>&1)
if [ $? -eq 0 ]; then
    defunct=$(echo "$defunct_output" | grep -E '^\s+value' | grep -Eo '[0-9]+')

    if [ -n "$defunct" ] && [ "$defunct" -ge 0 ]; then
        echo -e "${GREEN}✓ defunct sockets valid: $defunct${NC}"
        checks_passed=$((checks_passed + 1))
    else
        echo -e "${RED}✗ defunct sockets invalid: $defunct${NC}"
        checks_failed=$((checks_failed + 1))
    fi
else
    echo -e "${RED}✗ defunct sockets metric missing${NC}"
    checks_failed=$((checks_failed + 1))
fi

echo
echo "Checks passed: $checks_passed"
echo "Checks failed: $checks_failed"

if [ $checks_failed -eq 0 ]; then
    echo -e "${GREEN}✓ IPC metrics validation passed${NC}"
    exit 0
else
    echo -e "${RED}✗ IPC metrics validation failed${NC}"
    exit 1
fi
