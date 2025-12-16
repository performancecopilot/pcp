# macOS Uninstaller Testing Guide

This guide provides instructions for testing the updated PCP uninstaller on macOS.

## Prerequisites

- UTM or similar macOS VM environment
- Ability to build PCP .pkg installer
- sudo/admin access on test systems
- macOS versions to test (recommended):
  - macOS 11 (Big Sur) - First Apple Silicon release
  - macOS 12 (Monterey)
  - macOS 13 (Ventura)
  - macOS 14 (Sonoma)
  - macOS 15 (Sequoia)

## Building the Installer

```bash
cd /path/to/pcp
./configure --prefix=/usr --libexecdir=/usr/lib --sysconfdir=/etc --localstatedir=/var
make
./Makepkgs --verbose
```

The .dmg file will be created in `build/mac/` directory.

## Manual Testing Procedure

### Test 1: Fresh Install and Uninstall

This test verifies basic installation and uninstallation functionality.

#### Installation Phase
```bash
# 1. Mount the DMG
hdiutil attach pcp-X.Y.Z-BUILD.dmg

# 2. Install the .pkg
sudo installer -pkg /Volumes/pcp-X.Y.Z-BUILD/pcp-X.Y.Z-BUILD.pkg -target /

# 3. Verify installation
launchctl list | grep io.pcp
# Expected: Should show 4 services running:
#   io.pcp.pmcd
#   io.pcp.pmie
#   io.pcp.pmlogger
#   io.pcp.pmproxy

ls -la /Library/LaunchDaemons/io.pcp.*
# Expected: Should show 4 plist files

pkgutil --pkgs | grep pcp
# Expected: Should show "io.pcp.performancecopilot"

ls -d /etc/pcp /var/lib/pcp /var/log/pcp
# Expected: All three directories should exist

ls -la /usr/local/libexec/pcp/bin/uninstall-pcp
# Expected: uninstall-pcp script should exist and be executable
```

#### Uninstallation Phase
```bash
# 1. Run the uninstaller
sudo /usr/local/libexec/pcp/bin/uninstall-pcp
# When prompted, type 'y' to confirm

# 2. Verify services stopped
launchctl list | grep io.pcp
# Expected: No output (all services stopped)

# 3. Verify plists removed
ls -la /Library/LaunchDaemons/io.pcp.* 2>&1
# Expected: "No such file or directory"

# 4. Verify package receipt removed
pkgutil --pkgs | grep pcp
# Expected: No output

# 5. Verify config/log files still exist
ls -d /etc/pcp /var/lib/pcp /var/log/pcp
# Expected: All three directories should still exist

# 6. Verify uninstall script removed itself
ls -la /usr/local/libexec/pcp/bin/uninstall-pcp 2>&1
# Expected: "No such file or directory"

# 7. Verify warning message was displayed
# Expected: Should have seen message about config/log files remaining
```

### Test 2: Service Restart Before Uninstall

This test verifies that running services are properly stopped.

```bash
# 1. Install PCP (see Test 1)

# 2. Verify services are running
launchctl list io.pcp.pmcd
# Expected: Should show service info with PID

# 3. Run uninstaller
sudo /usr/local/libexec/pcp/bin/uninstall-pcp

# 4. Verify all services stopped (see Test 1 verification steps)
```

### Test 3: Multiple Install/Uninstall Cycles

This test verifies that the uninstaller can be run multiple times without issues.

```bash
# 1. Install PCP
# 2. Uninstall PCP
# 3. Install PCP again
# 4. Uninstall PCP again
# 5. Verify complete cleanup (see Test 1 verification steps)
```

### Test 4: Uninstall with Running Processes

This test verifies behavior when PCP processes are actively running.

```bash
# 1. Install PCP

# 2. Start using PCP tools
pminfo -f hinv.ncpu &
pmstat 1 &

# 3. Run uninstaller
sudo /usr/local/libexec/pcp/bin/uninstall-pcp

# 4. Verify services stopped despite active tools
launchctl list | grep io.pcp
# Expected: No output

# Note: PCP client tools may continue running briefly, but services should stop
```

### Test 5: Force Mode Uninstall

This test verifies that --force flag removes all PCP files including config/log directories.

```bash
# 1. Install PCP (see Test 1)

# 2. Run uninstaller with --force flag (no prompt)
sudo /usr/local/libexec/pcp/bin/uninstall-pcp --force
# Expected: No "Are you sure?" prompt

# 3. Verify complete removal
ls -d /etc/pcp /var/lib/pcp /var/log/pcp 2>&1
# Expected: "No such file or directory" for all three

# 4. Verify services stopped (see Test 1 verification steps)
launchctl list | grep io.pcp
# Expected: No output

# 5. Verify plists removed
ls -la /Library/LaunchDaemons/io.pcp.* 2>&1
# Expected: "No such file or directory"
```

### Test 6: Manual Config/Log Cleanup (Without Force)

This test verifies that users can manually clean up remaining files after standard uninstall.

```bash
# 1. Install and uninstall PCP without --force (see Test 1)

# 2. Verify config/log directories still exist
ls -d /etc/pcp /var/lib/pcp /var/log/pcp
# Expected: All three directories exist

# 3. Manually remove config/log directories
sudo rm -rf /etc/pcp /var/lib/pcp /var/log/pcp

# 4. Verify complete removal
ls -d /etc/pcp /var/lib/pcp /var/log/pcp 2>&1
# Expected: "No such file or directory"
```

### Test 7: Uninstall from Different Working Directories

This test verifies the safety check that prevents running from BINADM_DIR.

```bash
# 1. Install PCP

# 2. Try running from home directory (should work)
cd ~
sudo /usr/local/libexec/pcp/bin/uninstall-pcp
# Expected: Should proceed normally

# 3. Install again

# 4. Try running from BINADM_DIR (should fail)
cd /usr/local/libexec/pcp/bin
sudo ./uninstall-pcp
# Expected: Should display error "Do not run ... from BINADM_DIR"
```

### Test 8: Uninstall as Non-Root User

This test verifies the root user check.

```bash
# 1. Install PCP

# 2. Try running without sudo
/usr/local/libexec/pcp/bin/uninstall-pcp
# Expected: Should display error about needing root and suggest using sudo

# 3. Run with sudo (should work)
sudo /usr/local/libexec/pcp/bin/uninstall-pcp
```

## Automated Testing Ideas

### Option 1: Shell Script Test Suite

Create a test script that automates the verification steps:

```bash
#!/bin/bash
# test-uninstaller.sh

set -e

PKG_PATH="$1"
if [ -z "$PKG_PATH" ]; then
    echo "Usage: $0 /path/to/pcp-X.Y.Z-BUILD.pkg"
    exit 1
fi

echo "=== Test: Install PCP ==="
sudo installer -pkg "$PKG_PATH" -target /
sleep 5

echo "=== Verify: Services running ==="
if ! launchctl list | grep -q io.pcp.pmcd; then
    echo "FAIL: pmcd not running"
    exit 1
fi
echo "PASS: Services running"

echo "=== Test: Uninstall PCP ==="
echo "y" | sudo /usr/local/libexec/pcp/bin/uninstall-pcp
sleep 5

echo "=== Verify: Services stopped ==="
if launchctl list | grep -q io.pcp; then
    echo "FAIL: Services still running"
    exit 1
fi
echo "PASS: Services stopped"

echo "=== Verify: Plists removed ==="
if ls /Library/LaunchDaemons/io.pcp.* >/dev/null 2>&1; then
    echo "FAIL: Plists still present"
    exit 1
fi
echo "PASS: Plists removed"

echo "=== Verify: Package receipt removed ==="
if pkgutil --pkgs | grep -q pcp; then
    echo "FAIL: Package receipt still present"
    exit 1
fi
echo "PASS: Package receipt removed"

echo "=== Verify: Config/log files preserved ==="
if [ ! -d /etc/pcp ] || [ ! -d /var/lib/pcp ] || [ ! -d /var/log/pcp ]; then
    echo "FAIL: Config/log files removed"
    exit 1
fi
echo "PASS: Config/log files preserved"

echo ""
echo "=== ALL TESTS PASSED ==="
echo ""
echo "Cleaning up config/log files..."
sudo rm -rf /etc/pcp /var/lib/pcp /var/log/pcp
echo "Done"
```

### Option 2: UTM Snapshot-Based Testing

For testing across multiple macOS versions using UTM:

1. **Setup Phase**:
   - Create UTM VMs for each macOS version
   - Take a clean snapshot of each VM (before any PCP installation)
   - Name snapshots: `clean-macos-11`, `clean-macos-12`, etc.

2. **Test Execution**:
   ```bash
   # For each macOS version:
   # 1. Restore to clean snapshot
   # 2. Copy .pkg to VM (use shared folder or scp)
   # 3. SSH into VM and run test script
   # 4. Review results
   # 5. Restore to clean snapshot for next test
   ```

3. **Automation Script** (run on host):
   ```bash
   #!/bin/bash
   # test-all-macos-versions.sh

   VERSIONS=("11" "12" "13" "14" "15")
   PKG_PATH="/path/to/pcp-X.Y.Z-BUILD.pkg"

   for ver in "${VERSIONS[@]}"; do
       echo "Testing macOS $ver..."

       # Restore VM to clean snapshot (UTM CLI would be used here)
       # Start VM
       # Copy package to VM
       # SSH and run test script
       # Collect results
       # Stop VM

       echo "macOS $ver test complete"
   done
   ```

### Option 3: Bats Testing Framework

Use [Bats](https://github.com/bats-core/bats-core) for more structured testing:

```bash
# test-uninstaller.bats

setup() {
    PKG_PATH="${PKG_PATH:-/tmp/pcp.pkg}"
    sudo installer -pkg "$PKG_PATH" -target /
    sleep 5
}

teardown() {
    sudo rm -rf /etc/pcp /var/lib/pcp /var/log/pcp 2>/dev/null || true
}

@test "Services are running after install" {
    run launchctl list io.pcp.pmcd
    [ "$status" -eq 0 ]
}

@test "Uninstaller stops all services" {
    echo "y" | sudo /usr/local/libexec/pcp/bin/uninstall-pcp
    run launchctl list io.pcp.pmcd
    [ "$status" -ne 0 ]
}

@test "Uninstaller removes plists" {
    echo "y" | sudo /usr/local/libexec/pcp/bin/uninstall-pcp
    run ls /Library/LaunchDaemons/io.pcp.pmcd.plist
    [ "$status" -ne 0 ]
}

@test "Uninstaller preserves config files" {
    echo "y" | sudo /usr/local/libexec/pcp/bin/uninstall-pcp
    [ -d /etc/pcp ]
    [ -d /var/lib/pcp ]
    [ -d /var/log/pcp ]
}
```

## Regression Testing

After any changes to the uninstaller or build process:

1. Run Test 1 (Fresh Install and Uninstall) on at least two macOS versions
2. Run Test 2 (Service Restart) on one macOS version
3. Run Test 5 (Force Mode Uninstall) to verify complete cleanup
4. Run Test 8 (Non-Root User) to verify security checks

## Known Issues and Edge Cases

### Issue: Old PCP Installations
If testing on a system with a very old PCP installation (pre-launchd), manual cleanup may be required before testing.

### Issue: Disk Space
Ensure VMs have at least 5GB free space for installation and logging.

### Issue: VM Network
Some tests may require network access for downloading dependencies during PCP build.

## Test Results Template

```
Test Date: YYYY-MM-DD
Tester: [Your Name]
PCP Version: X.Y.Z-BUILD
macOS Version: [Version and Build Number]

Test 1 (Fresh Install/Uninstall): [PASS/FAIL]
  Notes:

Test 2 (Service Restart): [PASS/FAIL]
  Notes:

Test 3 (Multiple Cycles): [PASS/FAIL]
  Notes:

Test 4 (Running Processes): [PASS/FAIL]
  Notes:

Test 5 (Force Mode Uninstall): [PASS/FAIL]
  Notes:

Test 6 (Manual Cleanup): [PASS/FAIL]
  Notes:

Test 7 (Working Directory): [PASS/FAIL]
  Notes:

Test 8 (Non-Root User): [PASS/FAIL]
  Notes:

Overall Result: [PASS/FAIL]
Additional Comments:
```

## Continuous Integration Ideas

For future automation, consider:

1. **GitHub Actions with macOS Runners**:
   - Use GitHub's macOS runners
   - Run automated tests on every PR
   - Test on latest macOS only (to stay within CI budget)

2. **Tart VMs** (macOS on Apple Silicon):
   - Use [Tart](https://github.com/cirruslabs/tart) for lightweight macOS VMs
   - Automate VM creation and testing
   - Works well in CI/CD pipelines

3. **Homebrew Cask Testing**:
   - Use `brew install --cask` in CI
   - Run automated uninstall tests
   - Verify formula syntax with `brew audit`

## Quick Smoke Test (5 minutes)

For rapid testing during development:

### Standard Uninstall Test
```bash
# Build and install
sudo installer -pkg /path/to/pcp.pkg -target /

# Verify services running
launchctl list | grep io.pcp | wc -l  # Should be 4

# Uninstall (standard mode)
echo "y" | sudo /usr/local/libexec/pcp/bin/uninstall-pcp

# Verify cleanup
launchctl list | grep io.pcp | wc -l  # Should be 0
ls /Library/LaunchDaemons/io.pcp.* 2>&1 | grep -q "No such file"  # Should pass
[ -d /etc/pcp ] && echo "Config preserved" || echo "FAIL: Config removed"

# Manual cleanup
sudo rm -rf /etc/pcp /var/lib/pcp /var/log/pcp
```

### Force Mode Uninstall Test
```bash
# Build and install
sudo installer -pkg /path/to/pcp.pkg -target /

# Verify services running
launchctl list | grep io.pcp | wc -l  # Should be 4

# Uninstall (force mode - no prompt, complete removal)
sudo /usr/local/libexec/pcp/bin/uninstall-pcp --force

# Verify complete cleanup
launchctl list | grep io.pcp | wc -l  # Should be 0
ls /Library/LaunchDaemons/io.pcp.* 2>&1 | grep -q "No such file"  # Should pass
[ -d /etc/pcp ] && echo "FAIL: Config not removed" || echo "Complete removal successful"
```
