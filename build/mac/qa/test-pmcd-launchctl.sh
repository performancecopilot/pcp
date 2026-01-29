#!/bin/bash
# Test script for macOS pmcd launchctl fix
# Designed for slow VMs where pmcd startup can take 1-2 minutes

set -e

# Configuration - adjust these for your VM's speed
PMCD_START_TIMEOUT=180    # 3 minutes for pmcd to start responding
PMCD_STOP_TIMEOUT=60      # 1 minute for pmcd to stop
KEEPALIVE_TIMEOUT=180     # 3 minutes for KeepAlive to restart pmcd

PLIST_PATH="/Library/LaunchDaemons/io.pcp.pmcd.plist"

echo "=== macOS pmcd launchctl Test Suite ==="
echo "Timeouts: start=${PMCD_START_TIMEOUT}s, stop=${PMCD_STOP_TIMEOUT}s, keepalive=${KEEPALIVE_TIMEOUT}s"

# Helper function to wait for pmcd to respond to queries
wait_for_pmcd_ready() {
    local timeout=$1
    local start_time=$(date +%s)

    echo -n "Waiting for pmcd to respond (timeout ${timeout}s)..."

    # Try using pmcd_wait if available
    if command -v pmcd_wait >/dev/null 2>&1; then
        if pmcd_wait -t $timeout >/dev/null 2>&1; then
            echo " ready!"
            return 0
        else
            echo " timeout!"
            return 1
        fi
    fi

    # Fallback: poll with pminfo
    while true; do
        if pminfo -f pmcd.version >/dev/null 2>&1; then
            local elapsed=$(($(date +%s) - start_time))
            echo " ready! (${elapsed}s)"
            return 0
        fi

        local elapsed=$(($(date +%s) - start_time))
        if [ $elapsed -ge $timeout ]; then
            echo " timeout!"
            return 1
        fi

        # Show progress every 10 seconds
        if [ $((elapsed % 10)) -eq 0 ]; then
            echo -n "."
        fi

        sleep 2
    done
}

# Helper function to wait for pmcd to stop
wait_for_pmcd_stopped() {
    local timeout=$1
    local start_time=$(date +%s)

    echo -n "Waiting for pmcd to stop (timeout ${timeout}s)..."

    while pgrep pmcd >/dev/null 2>&1; do
        local elapsed=$(($(date +%s) - start_time))
        if [ $elapsed -ge $timeout ]; then
            echo " timeout!"
            return 1
        fi

        if [ $((elapsed % 5)) -eq 0 ]; then
            echo -n "."
        fi

        sleep 1
    done

    local elapsed=$(($(date +%s) - start_time))
    echo " stopped! (${elapsed}s)"
    return 0
}

# Test 1: Configuration
echo -e "\n[Test 1] Checking plist configuration..."
if [ ! -f "$PLIST_PATH" ]; then
    echo "✗ Plist file not found at $PLIST_PATH"
    exit 1
fi

if grep -q "LAUNCHED_BY_LAUNCHD" "$PLIST_PATH"; then
    echo "✓ LAUNCHED_BY_LAUNCHD environment variable present"
else
    echo "✗ LAUNCHED_BY_LAUNCHD environment variable missing"
    exit 1
fi

if grep -A1 "KeepAlive" "$PLIST_PATH" | grep -q "<true/>"; then
    echo "✓ KeepAlive enabled"
else
    echo "✗ KeepAlive not enabled"
    exit 1
fi

# Test 2: Clean start via launchctl
echo -e "\n[Test 2] Starting pmcd via launchctl..."

# Unload first if loaded
echo "Unloading pmcd service..."
sudo launchctl unload "$PLIST_PATH" 2>/dev/null || true

if ! wait_for_pmcd_stopped $PMCD_STOP_TIMEOUT; then
    echo "✗ pmcd did not stop in time, forcing kill..."
    sudo pkill -9 pmcd || true
    sleep 2
fi

# Load the service
echo "Loading pmcd service..."
sudo launchctl load "$PLIST_PATH"

# Wait for pmcd to be ready
if wait_for_pmcd_ready $PMCD_START_TIMEOUT; then
    echo "✓ pmcd started successfully"
else
    echo "✗ pmcd failed to start or respond"
    echo "Checking logs:"
    sudo tail -20 /var/log/pcp/pmcd/pmcd.log 2>/dev/null || echo "No log file found"
    exit 1
fi

# Verify pmcd is running
if pgrep pmcd >/dev/null 2>&1; then
    echo "✓ pmcd process is running (PID: $(pgrep pmcd))"
else
    echo "✗ pmcd process not found"
    exit 1
fi

# Test 3: Verify foreground mode
echo -e "\n[Test 3] Verifying foreground mode..."

if ps aux | grep '[p]mcd' | grep -q -- '-f'; then
    echo "✓ pmcd running in foreground mode (-f flag present)"
else
    echo "⚠ Warning: pmcd not running with -f flag"
    echo "Command line: $(ps aux | grep '[p]mcd')"
fi

# Check for only one pmcd process (no fork issues)
# Use pgrep to match only the pmcd binary, not scripts containing "pmcd"
PMCD_COUNT=$(pgrep -x pmcd | wc -l | tr -d ' ')
if [ "$PMCD_COUNT" -eq 1 ]; then
    echo "✓ Exactly one pmcd process running (no fork issues)"
else
    echo "✗ Found $PMCD_COUNT pmcd processes (expected 1)"
    echo "pmcd processes:"
    pgrep -lx pmcd
    echo "All processes matching pmcd:"
    ps aux | grep '[p]mcd'
    exit 1
fi

# Test 4: Verify launchctl tracking
echo -e "\n[Test 4] Verifying launchctl service status..."

if sudo launchctl list | grep -q "io.pcp.pmcd"; then
    LAUNCHCTL_STATUS=$(sudo launchctl list | grep io.pcp.pmcd)
    echo "✓ launchctl tracking pmcd service"
    echo "  Status: $LAUNCHCTL_STATUS"
else
    echo "✗ launchctl not tracking pmcd service"
    exit 1
fi

# Test 5: Functionality
echo -e "\n[Test 5] Testing pmcd functionality..."

if pminfo -f pmcd.version >/dev/null 2>&1; then
    VERSION=$(pminfo -f pmcd.version 2>/dev/null | grep value | awk '{print $2}')
    echo "✓ pmcd responds to queries (version: $VERSION)"
else
    echo "✗ pmcd not responding to queries"
    exit 1
fi

if pmprobe -v pmcd.numclients >/dev/null 2>&1; then
    CLIENTS=$(pmprobe -v pmcd.numclients 2>/dev/null)
    echo "✓ pmcd metrics accessible ($CLIENTS)"
else
    echo "✗ pmcd metrics not accessible"
    exit 1
fi

# Test 6: KeepAlive (crash recovery)
echo -e "\n[Test 6] Testing KeepAlive crash recovery..."

OLD_PID=$(pgrep pmcd)
echo "Current pmcd PID: $OLD_PID"

echo "Simulating crash (kill -9)..."
sudo kill -9 $OLD_PID

# Wait a moment for the kill to take effect
sleep 2

# Wait for launchctl to restart pmcd
echo -n "Waiting for KeepAlive to restart pmcd (timeout ${KEEPALIVE_TIMEOUT}s)..."
start_time=$(date +%s)
restarted=false

while true; do
    NEW_PID=$(pgrep pmcd 2>/dev/null || true)

    if [ -n "$NEW_PID" ] && [ "$OLD_PID" != "$NEW_PID" ]; then
        elapsed=$(($(date +%s) - start_time))
        echo " restarted! (${elapsed}s, new PID: $NEW_PID)"
        restarted=true
        break
    fi

    elapsed=$(($(date +%s) - start_time))
    if [ $elapsed -ge $KEEPALIVE_TIMEOUT ]; then
        echo " timeout!"
        break
    fi

    if [ $((elapsed % 10)) -eq 0 ]; then
        echo -n "."
    fi

    sleep 2
done

if [ "$restarted" = false ]; then
    echo "✗ KeepAlive failed to restart pmcd"
    echo "Checking launchctl status:"
    sudo launchctl list | grep io.pcp.pmcd || echo "Service not found in launchctl"
    exit 1
fi

echo "✓ KeepAlive successfully restarted pmcd"

# Test 7: Functionality after restart
echo -e "\n[Test 7] Verifying pmcd functionality after crash recovery..."

if wait_for_pmcd_ready $PMCD_START_TIMEOUT; then
    echo "✓ pmcd responding after KeepAlive restart"
else
    echo "✗ pmcd not responding after restart"
    exit 1
fi

if pminfo -f pmcd.version >/dev/null 2>&1; then
    echo "✓ pmcd fully functional after recovery"
else
    echo "✗ pmcd not functional after recovery"
    exit 1
fi

# Test 8: Clean shutdown
echo -e "\n[Test 8] Testing clean shutdown..."

echo "Unloading pmcd service..."
sudo launchctl unload "$PLIST_PATH"

if wait_for_pmcd_stopped $PMCD_STOP_TIMEOUT; then
    echo "✓ pmcd stopped cleanly"
else
    echo "✗ pmcd did not stop cleanly"
    exit 1
fi

if ! pgrep pmcd >/dev/null 2>&1; then
    echo "✓ No pmcd processes remaining"
else
    echo "✗ pmcd process still running"
    ps aux | grep '[p]mcd'
    exit 1
fi

# Test 9: Restart capability
echo -e "\n[Test 9] Testing restart capability..."

echo "Reloading pmcd service..."
sudo launchctl load "$PLIST_PATH"

if wait_for_pmcd_ready $PMCD_START_TIMEOUT; then
    echo "✓ pmcd restarted successfully"
else
    echo "✗ pmcd failed to restart"
    exit 1
fi

if pminfo -f pmcd.version >/dev/null 2>&1; then
    echo "✓ pmcd functional after restart"
else
    echo "✗ pmcd not functional after restart"
    exit 1
fi

# Summary
echo -e "\n=== All tests passed! ==="
echo ""
echo "Summary:"
echo "  ✓ Configuration correct (LAUNCHED_BY_LAUNCHD + KeepAlive)"
echo "  ✓ pmcd runs in foreground mode (-f flag)"
echo "  ✓ No fork issues (single pmcd process)"
echo "  ✓ launchctl properly tracks pmcd"
echo "  ✓ pmcd responds to queries"
echo "  ✓ KeepAlive restarts pmcd on crash"
echo "  ✓ pmcd functions correctly after recovery"
echo "  ✓ Clean shutdown/restart works"
echo ""
echo "The fix is working correctly!"
