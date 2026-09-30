#!/bin/bash
#
# Installed layout validation test
# Checks that the .pkg left the runtime directories owned by pcp:pcp, and
# that the primary pmlogger has registered itself (#2735). A root-owned
# $PCP_TMP_DIR/pmlogger stops pmlogger creating its "primary" link, so
# pmlogger_daily -K kills it as unregistered every 30 minutes.
#

set -u

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

echo "Testing installed layout..."

checks_passed=0
checks_failed=0

pass() {
    echo -e "${GREEN}✓ $1${NC}"
    checks_passed=$((checks_passed + 1))
}

fail() {
    echo -e "${RED}✗ $1${NC}"
    checks_failed=$((checks_failed + 1))
}

if [ ! -f /etc/pcp.conf ]; then
    fail "/etc/pcp.conf not found"
    exit 1
fi
PCP_TMP_DIR=$(. /etc/pcp.conf && echo "$PCP_TMP_DIR")

# Test 1: runtime directories the GNUmakefiles install as pcp:pcp
for dir in pmlogger pmie pmproxy mmv bash; do
    path="$PCP_TMP_DIR/$dir"
    if [ ! -d "$path" ]; then
        echo -e "${YELLOW}⚠ $path not installed, skipping${NC}"
        continue
    fi
    owner=$(/usr/bin/stat -f '%Su:%Sg' "$path")
    if [ "$owner" = "pcp:pcp" ]; then
        pass "$path owned by pcp:pcp"
    else
        fail "$path owned by $owner, expected pcp:pcp"
    fi
done

# Test 2: the primary pmlogger has registered, i.e. its "primary" link
# names a running process
if pgrep -x pmlogger > /dev/null; then
    primary="$PCP_TMP_DIR/pmlogger/primary"
    if [ -L "$primary" ]; then
        pid=$(basename "$(readlink "$primary")")
        if kill -0 "$pid" 2> /dev/null || ps -p "$pid" > /dev/null 2>&1; then
            pass "primary pmlogger registered (pid $pid)"
        else
            fail "$primary names pid $pid, which is not running"
        fi
    else
        fail "no $primary link, so the primary pmlogger never registered"
    fi
else
    echo -e "${YELLOW}⚠ no pmlogger running, skipping registration check${NC}"
fi

echo "Installed layout: $checks_passed passed, $checks_failed failed"
[ "$checks_failed" -eq 0 ]
