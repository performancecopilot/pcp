# macOS launchctl pmcd Test Plan

## Automated Testing (Recommended)

The automated test script handles all timing issues for slow VMs:

```bash
# Make executable and run
chmod +x test-pmcd-launchctl.sh
sudo ./test-pmcd-launchctl.sh
```

**Note**: The script is configured for slow VMs with 3-minute timeouts. You can adjust the timeout values at the top of the script if needed.

---

## Manual Testing

For manual validation on slow VMs, use these commands with proper wait strategies:

### Prerequisites
```bash
# Build and install PCP with the fix
cd /path/to/pcp-pmcd-daemon-launchctl
./configure --prefix=/usr --libexecdir=/usr/lib --sysconfdir=/etc --localstatedir=/var
make
sudo make install
```

### Helper Function for Waiting

Add this to your shell session for easier testing:

```bash
# Wait for pmcd to be ready (use PCP's built-in tool)
wait_pmcd() {
    echo "Waiting for pmcd to respond..."
    if command -v pmcd_wait >/dev/null 2>&1; then
        # Use PCP's pmcd_wait with 3 minute timeout
        pmcd_wait -t 180
    else
        # Fallback: poll until pmcd responds
        local count=0
        while ! pminfo -f pmcd.version >/dev/null 2>&1; do
            sleep 5
            count=$((count + 1))
            echo "Still waiting... (${count}x5s)"
            if [ $count -gt 36 ]; then  # 3 minutes
                echo "Timeout waiting for pmcd"
                return 1
            fi
        done
    fi
    echo "pmcd is ready!"
}
```

### Test 1: Verify Configuration Changes

```bash
# Check plist has environment variable and KeepAlive
cat /Library/LaunchDaemons/io.pcp.pmcd.plist | grep -A4 EnvironmentVariables
# Should show: LAUNCHED_BY_LAUNCHD = 1

cat /Library/LaunchDaemons/io.pcp.pmcd.plist | grep -A1 KeepAlive
# Should show: <true/>
```

### Test 2: Clean Start and Foreground Mode Verification

```bash
# Stop pmcd if running
sudo launchctl unload /Library/LaunchDaemons/io.pcp.pmcd.plist 2>/dev/null

# Wait for pmcd to fully stop
while pgrep pmcd >/dev/null 2>&1; do
    echo "Waiting for pmcd to stop..."
    sleep 2
done

# Load and start pmcd via launchctl
sudo launchctl load /Library/LaunchDaemons/io.pcp.pmcd.plist

# Wait for pmcd to be ready (this is the slow part - 1-2 minutes on slow VM)
wait_pmcd

# Verify pmcd is running
ps aux | grep pmcd | grep -v grep

# Check that pmcd is running with -f flag (foreground mode)
ps aux | grep '[p]mcd' | grep -- '-f'
# Should see the -f flag in the command line

# Verify exactly one pmcd process (no fork issues)
echo "pmcd process count: $(ps aux | grep '[p]mcd' | wc -l | tr -d ' ')"
# Should be 1

# Check launchctl service status
sudo launchctl list | grep io.pcp.pmcd
# Should show running status with PID
```

### Test 3: Verify pmcd Functionality

```bash
# Test basic metric queries
pminfo -f hinv.ncpu
pminfo -f kernel.all.load
pminfo -f pmcd.version

# Verify pmcd responds
pmprobe -v pmcd.numclients
# Should return: pmcd.numclients 1 <number>

# Check pmcd log for errors
sudo tail -50 /var/log/pcp/pmcd/pmcd.log
# Should not show fork-related errors or startup failures
```

### Test 4: Verify KeepAlive Behavior (Crash Recovery)

```bash
# Get pmcd PID
PMCD_PID=$(pgrep pmcd)
echo "pmcd PID: $PMCD_PID"

# Forcefully kill pmcd to simulate crash
sudo kill -9 $PMCD_PID

# Wait for launchctl to restart pmcd (can take 1-2 minutes on slow VM)
echo "Waiting for KeepAlive to restart pmcd..."
sleep 10

# Poll for new pmcd process
for i in {1..24}; do  # 2 minutes max
    NEW_PID=$(pgrep pmcd)
    if [ -n "$NEW_PID" ] && [ "$PMCD_PID" != "$NEW_PID" ]; then
        echo "✓ pmcd restarted with new PID: $NEW_PID (after ${i}x5s)"
        break
    fi
    echo "Attempt $i: waiting..."
    sleep 5
done

# Verify new PID is different
NEW_PID=$(pgrep pmcd)
if [ "$PMCD_PID" != "$NEW_PID" ] && [ -n "$NEW_PID" ]; then
    echo "✓ KeepAlive working - pmcd was restarted"
else
    echo "✗ KeepAlive failed - pmcd was not restarted"
fi

# Wait for pmcd to be ready after restart
wait_pmcd

# Verify pmcd still works after restart
pminfo -f pmcd.version
```

### Test 5: Check Logs

```bash
# Check system log for launchctl messages about pmcd (macOS 10.12+)
log show --predicate 'process == "launchd"' --last 10m | grep pmcd

# Check pmcd logs
sudo tail -100 /var/log/pcp/pmcd/pmcd.log

# Check launchctl stderr/stdout
sudo cat /var/log/pcp/pmcd/plist.stderr
sudo cat /var/log/pcp/pmcd/plist.stdout
```

### Test 6: Clean Shutdown and Restart

```bash
# Stop pmcd cleanly via launchctl
sudo launchctl unload /Library/LaunchDaemons/io.pcp.pmcd.plist

# Wait for pmcd to stop (should be quick)
while pgrep pmcd >/dev/null 2>&1; do
    echo "Waiting for pmcd to stop..."
    sleep 2
done
echo "pmcd stopped"

# Verify pmcd stopped
pgrep pmcd
# Should return nothing

# Restart pmcd
sudo launchctl load /Library/LaunchDaemons/io.pcp.pmcd.plist

# Wait for pmcd to be ready (1-2 minutes on slow VM)
wait_pmcd

# Verify it started successfully
pminfo -f pmcd.version
```

---

## Expected Results Summary

✅ **Configuration**: LAUNCHED_BY_LAUNCHD env var present, KeepAlive = true
✅ **Foreground mode**: pmcd runs with `-f` flag
✅ **No fork issues**: Only one pmcd process exists
✅ **launchctl tracking**: Service shows in `launchctl list` with PID
✅ **KeepAlive works**: pmcd automatically restarts after crash
✅ **Functionality**: pmcd responds to metric queries normally
✅ **Clean logs**: No errors about fork failures or unexpected exits

---

## Troubleshooting

### pmcd takes a very long time to start
This is normal on slow VMs. The automated test script has 3-minute timeouts. You can increase them if needed.

### KeepAlive doesn't restart pmcd
Check launchctl status:
```bash
sudo launchctl list | grep io.pcp.pmcd
```

Check for errors:
```bash
sudo tail -100 /var/log/pcp/pmcd/pmcd.log
sudo cat /var/log/pcp/pmcd/plist.stderr
```

### pmcd not running in foreground mode
Verify the environment variable is set:
```bash
grep -A4 EnvironmentVariables /Library/LaunchDaemons/io.pcp.pmcd.plist
```

Check if rc_pmcd is detecting it:
```bash
sudo launchctl unload /Library/LaunchDaemons/io.pcp.pmcd.plist
sudo launchctl load /Library/LaunchDaemons/io.pcp.pmcd.plist
wait_pmcd
ps aux | grep '[p]mcd'  # Should show -f flag
```
