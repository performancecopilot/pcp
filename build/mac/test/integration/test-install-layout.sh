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

# Test 1: runtime directories the GNUmakefiles install as pcp:pcp. The package
# always ships the first three; mmv and bash are created later by their PMDA
# Install scripts, so a fresh package legitimately lacks them.
required_dirs="pmlogger pmie pmproxy"
optional_dirs="mmv bash"

check_dir_owner() {
    local path="$1" owner
    owner=$(/usr/bin/stat -f '%Su:%Sg' "$path")
    if [ "$owner" = "pcp:pcp" ]; then
        pass "$path owned by pcp:pcp"
    else
        fail "$path owned by $owner, expected pcp:pcp"
    fi
}

for dir in $required_dirs; do
    path="$PCP_TMP_DIR/$dir"
    if [ -d "$path" ]; then
        check_dir_owner "$path"
    else
        fail "$path is not an installed runtime directory"
    fi
done

for dir in $optional_dirs; do
    path="$PCP_TMP_DIR/$dir"
    if [ -d "$path" ]; then
        check_dir_owner "$path"
    else
        echo -e "${YELLOW}⚠ $path not installed, skipping${NC}"
    fi
done

# True when $1 is the pid of a running pmlogger. A bare "is it running" check
# would pass a stale link whose pid the OS has since reused for another process.
is_pmlogger_pid() {
    case "$1" in
        ''|*[!0-9]*) return 1 ;;
    esac
    local command
    command=$(ps -p "$1" -o comm= 2> /dev/null) || return 1
    [ "$(basename "$command")" = "pmlogger" ]
}

# Test 2: the primary pmlogger has registered, i.e. its "primary" link
# names a running pmlogger process
if pgrep -x pmlogger > /dev/null; then
    primary="$PCP_TMP_DIR/pmlogger/primary"
    if [ -L "$primary" ]; then
        pid=$(basename "$(readlink "$primary")")
        if is_pmlogger_pid "$pid"; then
            pass "primary pmlogger registered (pid $pid)"
        else
            fail "$primary names pid $pid, which is not a running pmlogger"
        fi
    else
        fail "no $primary link, so the primary pmlogger never registered"
    fi
else
    echo -e "${YELLOW}⚠ no pmlogger running, skipping registration check${NC}"
fi

echo "Installed layout: $checks_passed passed, $checks_failed failed"
[ "$checks_failed" -eq 0 ]
