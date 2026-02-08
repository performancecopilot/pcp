# macOS QA Testing Implementation Plan

## Overview

Enable QA testing for PCP on macOS through:
1. Fixing the QA infrastructure's service management functions for modern macOS (launchctl)
2. Adding a Cirrus CI QA task dependent on the main build task
3. Creating a separate GitHub workflow for macOS QA (designed to fold into main qa.yml later)

---

## Status (2026-02-08)

### ✅ Completed

**Service Management Infrastructure Fixes** (Commit: 49ee3618e9)
- Fixed pmlogger/pmie launchd plists with `StartInterval` for periodic health checks
- Updated postinstall script to bootstrap and kickstart pmlogger/pmie services
- Fixed `is_chkconfig_on()` in `src/pmcd/rc-proc.sh` - replaced ancient `/etc/hostconfig` check with modern `launchctl print-disabled`
- Added localhost DNS verification to CI workflow
- Added pmlogger/pmie startup to CI workflow

**Verification**: All new CI steps pass:
- ✓ Verify localhost DNS
- ✓ Start pmcd
- ✓ Start pmlogger (NEW - first time working!)
- ✓ Start pmie (NEW - first time working!)

### ✅ RESOLVED: DYLD_LIBRARY_PATH for QA Test Binaries

**Status**: Fixed in commit 39ad5306c1 (2026-02-08)

**Problem**: QA tests failed because test binaries in `qa/src/` couldn't find `libpcp.dylib`:
```
dyld[51771]: Library not loaded: libpcp.4.dylib
  Referenced from: /Users/runner/work/pcp/pcp/qa/src/pducheck
  Reason: tried: 'libpcp.4.dylib' (no such file)
Abort trap: 6
```

**Impact**: 44 of 70 sanity tests failed (exit code 44 in CI run 21793886742)

**Solution Implemented**:
- Extended Darwin case block in `qa/common.rc` to set DYLD_LIBRARY_PATH from $PCP_LIB_DIR
- Added `_add_lib_path()` cross-platform helper function for mock library tests
- Updated qa/744, qa/745, qa/1996 to use the new helper
- Added CI verification step in `.github/workflows/qa-macos.yml` to validate library paths before running tests

**Files Modified**:
- `qa/common.rc` - Added DYLD_LIBRARY_PATH setup and _add_lib_path() helper
- `qa/744`, `qa/745`, `qa/1996` - Converted to use _add_lib_path()
- `.github/workflows/qa-macos.yml` - Added pre-test library path verification

**Verification**: Next CI run will show if dyld errors are resolved

---

## Phase 0: Fix DYLD_LIBRARY_PATH for Test Binaries (CRITICAL - HIGH PRIORITY)

### Problem Statement

Test binaries compiled in `qa/src/` are linked against PCP shared libraries but cannot load them at runtime on macOS because `DYLD_LIBRARY_PATH` is not set. This causes immediate crashes with "Library not loaded" errors.

### 0.1 Set DYLD_LIBRARY_PATH in qa/common.rc

**File**: `qa/common.rc`
**Location**: In the environment setup section (around where `PCP_PLATFORM` is detected)

**Add Darwin-specific library path setup**:
```bash
# macOS: Set DYLD_LIBRARY_PATH for test binaries to find PCP libraries
if [ "$PCP_PLATFORM" = "darwin" ]
then
    # Check common installation locations
    if [ -d /usr/local/lib ]
    then
        DYLD_LIBRARY_PATH="/usr/local/lib${DYLD_LIBRARY_PATH:+:}${DYLD_LIBRARY_PATH}"
        export DYLD_LIBRARY_PATH
    fi

    # Also check build tree if running from source
    if [ -d "$PCP_DIR/src/libpcp/src/.libs" ]
    then
        DYLD_LIBRARY_PATH="$PCP_DIR/src/libpcp/src/.libs${DYLD_LIBRARY_PATH:+:}${DYLD_LIBRARY_PATH}"
        export DYLD_LIBRARY_PATH
    fi
fi
```

**Rationale**:
- macOS uses `DYLD_LIBRARY_PATH` (not `LD_LIBRARY_PATH`) for dynamic library loading
- Test binaries in `qa/src/` are built with `-lpcp` but don't have rpath set
- PCP libraries are installed to `/usr/local/lib` on macOS
- This mirrors Linux behavior where `LD_LIBRARY_PATH` would be set implicitly

### 0.2 Alternative: Fix Test Binary Linking (Lower Priority)

**File**: `qa/src/GNUmakefile`

**Option**: Add `-Wl,-rpath,/usr/local/lib` to LDFLAGS for Darwin builds

**Pros**: More robust, doesn't rely on environment variables
**Cons**: Requires rebuild of all test binaries, more invasive change

**Recommendation**: Do Phase 0.1 first for immediate fix, consider this for long-term solution.

### 0.3 Update GitHub Workflow

**File**: `.github/workflows/qa-macos.yml`
**Location**: Before "Run QA sanity tests" step

**Add library path verification**:
```yaml
      - name: Verify library paths
        run: |
          echo "=== Checking PCP library installation ==="
          ls -la /usr/local/lib/libpcp* || echo "No libpcp in /usr/local/lib"

          echo ""
          echo "Setting DYLD_LIBRARY_PATH..."
          export DYLD_LIBRARY_PATH="/usr/local/lib"

          echo ""
          echo "Testing a QA binary..."
          cd qa/src
          if [ -f pducheck ]; then
            otool -L pducheck | grep libpcp
            DYLD_LIBRARY_PATH=/usr/local/lib ./pducheck -? || echo "pducheck test"
          fi
```

**Note**: `qa/common.rc` should handle this, but adding explicit verification helps debug CI issues.

### Verification Steps

After implementing Phase 0.1:

1. **Local test** (on macOS with PCP installed):
```bash
cd qa
. ./common.rc
echo "DYLD_LIBRARY_PATH=$DYLD_LIBRARY_PATH"
./src/pducheck -?  # Should run without dyld errors
```

2. **Run qa/001**:
```bash
cd qa
./check 001  # Should pass - this was failing with dyld errors
```

3. **Run sanity suite**:
```bash
cd qa
./check -g sanity -x not_in_ci
```

**Expected Result**: Significantly reduced test failures (from 44/70 to < 10/70)

---

## Phase 1: Fix QA Infrastructure for macOS

### 1.1 Fix `qa/admin/myconfigure` Darwin Block

**File**: `qa/admin/myconfigure`
**Lines**: 129-135

**Current (broken)**:
```bash
elif [ $target = darwin ]
then
    CC=clang; export CC
    CXX=clang; export CXX
    configopts="`brew diy --version=$version --name=pcp`"
```

**Replace with** (matches Makepkgs):
```bash
elif [ $target = darwin ]
then
    # Match Makepkgs Darwin configuration
    CC=clang; export CC
    CXX=clang; export CXX
    etc=`realpath /etc`
    var=`realpath /var`
    configopts="--sysconfdir=$etc --localstatedir=$var --prefix=/usr/local --with-qt=no"
```

### 1.2 Add Common Helper Function for Service-to-Plist Mapping

**File**: `qa/common.check`
**Location**: Add near the top of the service management section (around line 250)

**Add new helper function**:
```bash
# Map PCP service name to launchd plist name for macOS
# Usage: __plist=$(_darwin_plist_name pmcd)
# Returns empty string for "verbose" or unknown services
#
_darwin_plist_name()
{
    case "$1"
    in
        pmcd)       echo "io.pcp.pmcd" ;;
        pmlogger)   echo "io.pcp.pmlogger" ;;
        pmie)       echo "io.pcp.pmie" ;;
        pmproxy)    echo "io.pcp.pmproxy" ;;
        verbose)    echo "" ;;
        *)          echo "io.pcp.$1" ;;
    esac
}
```

### 1.3 Add launchctl Support to `_service()` Function

**File**: `qa/common.check`
**Location**: Insert after line 360 (after `esac`), before line 361 (`elif [ -f $PCP_RC_DIR/$1 ]`)

**Insert Darwin-specific launchctl block**:
```bash
    # macOS launchctl handling - inserted before RC script fallback
    #
    elif [ "$PCP_PLATFORM" = "darwin" ]
    then
        __plist_name=$(_darwin_plist_name "$1")
        __plist_path="/Library/LaunchDaemons/$__plist_name.plist"

        if [ -n "$__plist_name" ] && [ -f "$__plist_path" ]
        then
            case "$2"
            in
                start)
                    $__verbose && echo "_service: using launchctl for $__plist_name start"
                    if launchctl list "$__plist_name" >/dev/null 2>&1
                    then
                        $__verbose && echo "_service: $__plist_name already loaded, kickstarting"
                        $sudo launchctl kickstart -p system/$__plist_name 2>>$seq_full
                    else
                        $sudo launchctl bootstrap system "$__plist_path" 2>>$seq_full
                        $sudo launchctl kickstart -p system/$__plist_name 2>>$seq_full
                    fi
                    ;;
                stop)
                    $__verbose && echo "_service: using launchctl for $__plist_name stop"
                    if launchctl list "$__plist_name" >/dev/null 2>&1
                    then
                        $sudo launchctl bootout system/$__plist_name 2>>$seq_full || true
                    fi
                    ;;
                restart)
                    $__verbose && echo "_service: using launchctl for $__plist_name restart"
                    if launchctl list "$__plist_name" >/dev/null 2>&1
                    then
                        $sudo launchctl bootout system/$__plist_name 2>>$seq_full || true
                    fi
                    sleep 1
                    $sudo launchctl bootstrap system "$__plist_path" 2>>$seq_full
                    $sudo launchctl kickstart -p system/$__plist_name 2>>$seq_full
                    ;;
                status)
                    if launchctl list "$__plist_name" >/dev/null 2>&1
                    then
                        return 0
                    else
                        return 1
                    fi
                    ;;
                *)
                    echo "_service: unknown action '$2' for launchctl" >&2
                    return 1
                    ;;
            esac
            return 0
        fi
        # Fall through to RC script if no plist exists
```

### 1.4 Replace `_change_config()` Darwin Block

**File**: `qa/common.check`
**Lines**: 2835-2875

**Replace entire Darwin block with**:
```bash
    elif [ $PCP_PLATFORM = darwin ]
    then
        # Modern macOS uses launchctl for service configuration
        __plist=$(_darwin_plist_name "$1")
        if [ -n "$__plist" ]
        then
            __plist_path="/Library/LaunchDaemons/$__plist.plist"
            if [ ! -f "$__plist_path" ]
            then
                echo "_change_config: Error: plist not found at $__plist_path"
                return 1
            fi
            if [ "$2" = "on" ]
            then
                # Enable and load
                $sudo launchctl load -w "$__plist_path" 2>/dev/null || true
            elif [ "$2" = "off" ]
            then
                # Disable and unload
                $sudo launchctl unload -w "$__plist_path" 2>/dev/null || true
            else
                echo "_change_config: Error: bad state ($2) should be on or off"
                return 1
            fi
        fi
```

### 1.5 Replace `_get_config()` Darwin Block

**File**: `qa/common.check`
**Lines**: 3066-3099

**Replace entire Darwin block with**:
```bash
    elif [ $PCP_PLATFORM = darwin ]
    then
        # Modern macOS uses launchctl for service status
        __plist=$(_darwin_plist_name "$1")
        if [ -z "$__plist" ]
        then
            echo on
        else
            if launchctl list "$__plist" >/dev/null 2>&1
            then
                echo on
            else
                echo off
            fi
        fi
```

---

## Phase 2: Add Cirrus CI QA Task

**File**: `.cirrus.yml`
**Location**: After line 81 (after `pause_script` block)

**Key insight**: Since `qa_task` depends on `macOS PCP Build`, the VM state is preserved. PCP is already installed, pmcd is running, TCP stats are enabled. We only need to:
1. Create the pcpqa user for test execution
2. Initialize QA
3. Run the tests

**Add new task**:
```yaml
qa_task:
  name: macOS PCP QA Tests
  depends_on:
    - macOS PCP Build
  # Same VM image - state preserved from build task
  macos_instance:
    image: ghcr.io/cirruslabs/macos-tahoe-base:latest
  env:
    PCP_QA_ARGS: "-g sanity -x not_in_ci"
  # PCP is already installed from the dependent build task
  # pmcd is already running, TCP stats already enabled
  create_pcpqa_user_script: |
    if ! dscl . -list /Users | grep -q '^pcpqa$'; then
      sudo dscl . -create /Users/pcpqa
      sudo dscl . -create /Users/pcpqa UniqueID 2050
      sudo dscl . -create /Users/pcpqa UserShell /bin/bash
      sudo dscl . -create /Users/pcpqa RealName 'PCP QA User'
      sudo dscl . -create /Users/pcpqa NFSHomeDirectory /var/lib/pcp/testsuite
      sudo dscl . -create /Users/pcpqa PrimaryGroupID 20
      sudo dscl . -create /Users/pcpqa Password \*
    fi
    echo 'pcpqa ALL=(ALL) NOPASSWD: ALL' | sudo tee /etc/sudoers.d/pcpqa
    sudo chmod 0440 /etc/sudoers.d/pcpqa
    sudo chown -R pcpqa /var/lib/pcp/testsuite || true
  verify_pcp_script: |
    # Quick sanity check that PCP is running from the build task
    pcp || (echo "ERROR: pmcd not running - build task may have failed"; exit 1)
  init_qa_script: |
    cd /var/lib/pcp/testsuite
    sudo -u pcpqa ./check 002 || true
  run_qa_script: |
    cd /var/lib/pcp/testsuite
    sudo -u pcpqa ./check -TT ${PCP_QA_ARGS} 2>&1 || true
  always:
    qa_logs_artifacts:
      path: "/var/lib/pcp/testsuite/*.out.bad"
    qa_full_artifacts:
      path: "/var/lib/pcp/testsuite/*.full"
    qa_timings_artifacts:
      path: "/var/lib/pcp/testsuite/check.timings"
```

---

## Phase 3: Create GitHub Workflow for macOS QA

**File**: `.github/workflows/qa-macos.yml` (new file)

**Note**: GitHub Actions doesn't preserve VM state between jobs like Cirrus CI does, so we need the full build->install->qa flow here.

```yaml
# macOS QA Tests - designed to fold into qa.yml later
name: macOS QA
on:
  workflow_dispatch:
    inputs:
      pcp_qa_args:
        description: 'QA ./check arguments'
        default: '-g sanity -x not_in_ci'

jobs:
  build:
    name: Build macOS Package
    runs-on: macos-latest
    steps:
      - uses: actions/checkout@v6

      - name: Install dependencies
        run: |
          brew update
          brew install autoconf unixodbc valkey libuv coreutils gnu-tar pkg-config python3 python-setuptools || true

      - name: Build PCP
        run: ./Makepkgs --verbose

      - name: Upload DMG artifact
        uses: actions/upload-artifact@v6
        with:
          name: pcp-macos-dmg
          path: pcp-*/build/mac/pcp-*.dmg
          retention-days: 1

  qa:
    name: QA Tests
    needs: build
    runs-on: macos-latest
    steps:
      - uses: actions/checkout@v6

      - name: Download DMG artifact
        uses: actions/download-artifact@v6
        with:
          name: pcp-macos-dmg
          path: ./dmg-artifact

      - name: Enable TCP stats
        run: sudo sysctl -w net.inet.tcp.disable_access_to_stats=0

      - name: Install PCP
        run: |
          DMG_PATH=$(find ./dmg-artifact -name "pcp-*.dmg" | head -1)
          MOUNT_OUTPUT=$(hdiutil attach "$DMG_PATH" | tail -1)
          VOLUME_PATH=$(echo "$MOUNT_OUTPUT" | awk '{print $3}')
          PKG_PATH=$(find "$VOLUME_PATH" -name "*.pkg" | head -1)
          sudo installer -pkg "$PKG_PATH" -target / -verbose
          hdiutil detach "$VOLUME_PATH" || true

      - name: Wait for pmcd
        run: |
          TIMEOUT=180
          ELAPSED=0
          while [ $ELAPSED -lt $TIMEOUT ]; do
            if pcp 2>/dev/null; then break; fi
            sleep 5
            ELAPSED=$((ELAPSED + 5))
          done

      - name: Create pcpqa user
        run: |
          if ! dscl . -list /Users | grep -q '^pcpqa$'; then
            sudo dscl . -create /Users/pcpqa
            sudo dscl . -create /Users/pcpqa UniqueID 2050
            sudo dscl . -create /Users/pcpqa UserShell /bin/bash
            sudo dscl . -create /Users/pcpqa RealName 'PCP QA User'
            sudo dscl . -create /Users/pcpqa NFSHomeDirectory /var/lib/pcp/testsuite
            sudo dscl . -create /Users/pcpqa PrimaryGroupID 20
          fi
          echo 'pcpqa ALL=(ALL) NOPASSWD: ALL' | sudo tee /etc/sudoers.d/pcpqa
          sudo chown -R pcpqa /var/lib/pcp/testsuite || true

      - name: Initialize QA
        run: |
          cd /var/lib/pcp/testsuite
          sudo -u pcpqa ./check 002 || true

      - name: Run QA tests
        run: |
          cd /var/lib/pcp/testsuite
          sudo -u pcpqa ./check -TT ${{ github.event.inputs.pcp_qa_args || '-g sanity -x not_in_ci' }} 2>&1 | tee /tmp/test.log || true

      - name: Collect artifacts
        if: always()
        run: |
          mkdir -p artifacts/test
          cp /var/lib/pcp/testsuite/check.timings artifacts/test/ || true
          cp /tmp/test.log artifacts/test/ || true
          for t in /var/lib/pcp/testsuite/*.out.bad; do
            [ -f "$t" ] && cp "$t" artifacts/test/
          done

      - name: Upload test results
        if: always()
        uses: actions/upload-artifact@v6
        with:
          name: test-macos
          path: artifacts/test
```

---

## Implementation Order

### Current Priority (As of 2026-02-08)

**CRITICAL PATH**: Phase 0 must be completed first - all QA tests are currently blocked by DYLD_LIBRARY_PATH issue.

1. **Phase 0.1**: 🚨 **FIX DYLD_LIBRARY_PATH** in `qa/common.rc` - **BLOCKING ALL TESTS**
2. **Phase 0.3**: Add library path verification to CI workflow
3. **Phase 1.1**: `qa/admin/myconfigure` - No dependencies
4. **Phase 1.2**: Add `_darwin_plist_name()` helper - No dependencies
5. **Phase 1.3-1.5**: Update service functions to use helper - Depends on 1.2
6. **Phase 2**: `.cirrus.yml` QA task - Depends on Phase 0 + Phase 1
7. **Phase 3**: GitHub workflow - Depends on Phase 0 + Phase 1

### Original Order (Pre-DYLD Discovery)

1. **Phase 1.1**: `qa/admin/myconfigure` - No dependencies
2. **Phase 1.2**: Add `_darwin_plist_name()` helper - No dependencies
3. **Phase 1.3-1.5**: Update service functions to use helper - Depends on 1.2
4. **Phase 2**: `.cirrus.yml` QA task - Depends on Phase 1
5. **Phase 3**: GitHub workflow - Depends on Phase 1

---

## Verification

### Manual Testing (Phase 1)

After modifying `qa/common.check`, test locally on macOS:

```bash
# Source the functions
. /etc/pcp.conf
cd /path/to/pcp/qa
. ./common.check

# Test the helper function
_darwin_plist_name pmcd      # should output: io.pcp.pmcd
_darwin_plist_name pmlogger  # should output: io.pcp.pmlogger
_darwin_plist_name verbose   # should output: (empty)

# Test _service
_service -v pmcd status
_service -v pmcd stop
_service -v pmcd start
_service -v pmcd restart

# Test _get_config
_get_config pmcd    # should return "on" or "off"
_get_config pmlogger

# Test _change_config
_change_config pmcd off
_get_config pmcd    # should return "off"
_change_config pmcd on
_get_config pmcd    # should return "on"
```

### Run QA Test 002

```bash
cd /var/lib/pcp/testsuite
./check 002
```

### Run Sanity Suite

```bash
cd /var/lib/pcp/testsuite
./check -g sanity
```

---

## Files to Modify

| File | Change |
|------|--------|
| `qa/admin/myconfigure` | Replace Darwin `brew diy` block |
| `qa/common.check` | Add `_darwin_plist_name()` helper, add launchctl to `_service()`, replace `_change_config()` and `_get_config()` Darwin blocks |
| `.cirrus.yml` | Add `qa_task` dependent on main build |
| `.github/workflows/qa-macos.yml` | New file - macOS QA workflow |

---

## Pre-Merge Checklist

**IMPORTANT**: Before merging to main, ensure:

1. **Remove temporary push trigger from `.github/workflows/qa-macos.yml`**
   - The workflow should only trigger on `workflow_dispatch:` in the final version
   - Current temporary config includes `push: branches: [macos-qa-uplift]` for testing
   - Remove the entire `push:` block before merge to keep it manual-trigger only

---

## Known Issues & Technical Debt

### 🚨 CRITICAL: DYLD_LIBRARY_PATH Not Set (HIGH PRIORITY - BLOCKING QA TESTS)

**Status**: Discovered 2026-02-08, needs immediate fix

**Problem**: Test binaries in `qa/src/` cannot load PCP shared libraries at runtime because `DYLD_LIBRARY_PATH` is not set on macOS.

**Impact**:
- 44 of 70 sanity tests fail with dyld errors
- Test execution is completely blocked
- CI run 21793886742 demonstrates the issue

**Error Message**:
```
dyld[51771]: Library not loaded: libpcp.4.dylib
  Referenced from: <UUID> /Users/runner/work/pcp/pcp/qa/src/pducheck
  Reason: tried: 'libpcp.4.dylib' (no such file)
Abort trap: 6
```

**Root Cause Analysis**:
- macOS uses `DYLD_LIBRARY_PATH` (not `LD_LIBRARY_PATH`) for dynamic library search
- `qa/common.rc` and `qa/common.check` never set this variable
- Test binaries are linked with `-lpcp` but don't have rpath embedded
- PCP libraries are installed to `/usr/local/lib` on macOS

**Fix**: See Phase 0 above for implementation details

**Files Affected**:
- `qa/common.rc` - needs DYLD_LIBRARY_PATH setup
- `qa/src/GNUmakefile` - alternative: add rpath to LDFLAGS

**Priority**: CRITICAL - blocks all QA test execution on macOS

---

### ✅ RESOLVED: Service Management Infrastructure

**Status**: Fixed in commit 49ee3618e9 (2026-02-08)

**Problems Fixed**:
1. pmlogger/pmie services never started - plists existed but were never bootstrapped
2. `is_chkconfig_on()` in `src/pmcd/rc-proc.sh` used `/etc/hostconfig` (dead since macOS 10.6/2009)
3. Slow pmcd startup due to potential localhost DNS issues

**Solution Implemented**:
- Added `StartInterval: 600` to pmlogger/pmie plists for periodic health checks (like systemd timers)
- Updated postinstall script to bootstrap and kickstart pmlogger/pmie services
- Replaced `/etc/hostconfig` check with modern `launchctl print-disabled`
- Added localhost DNS verification step in CI workflow

**Verification**: CI run 21793886742 shows all services starting successfully

---

### PMNS Automatic Rebuild Not Working in CI

**Status**: Workaround implemented, root cause investigation needed

**Problem**: The PMNS (Performance Metrics Name Space) `root` file should be automatically rebuilt when pmcd starts via the rc script's `_reboot_setup()` function, but this isn't happening reliably in CI environments.

**Current Workaround**: Explicitly run `/var/lib/pcp/pmns/Rebuild -v` after `make install` and before starting pmcd (implemented in `.github/workflows/qa-macos.yml`).

**Why it should work automatically**:
1. The launchd plist (`build/mac/io.pcp.pmcd.plist`) correctly calls `/etc/init.d/pmcd start`
2. The rc script (`src/pmcd/rc_pmcd`) calls `_reboot_setup()` which should trigger the Rebuild
3. The Rebuild should trigger if ANY of these conditions are met:
   - `.NeedRebuild` file exists in `/var/lib/pcp/pmns/`
   - `root` file doesn't exist
   - Any `root_*` files are newer than `root`

**Investigation needed**:
- Is `.NeedRebuild` actually being installed during `make install`?
  - The makefile condition `ifeq (, $(filter redhat debian suse, $(PACKAGE_DISTRIBUTION)))` should install it for macOS
  - Verify `PACKAGE_DISTRIBUTION=macos` in CI builds
- Is `_reboot_setup()` actually being called when launchctl starts the service?
  - Check if KeepAlive/crash recovery bypasses the rc script
  - Review launchd logs/stderr output
- Is the Rebuild script failing silently?
  - Check if pmnsmerge and other tools are in PATH
  - Verify permissions on PMNS directory

**Maintainer's manual workaround** (for reference):
```bash
. /etc/pcp.conf
sudo touch $PCP_VAR_DIR/pmns/.NeedRebuild
sudo /etc/init.d/pmcd restart
```

**Files to investigate**:
- `src/pmns/GNUmakefile` (lines 62-64) - .NeedRebuild installation
- `src/pmcd/rc_pmcd` (lines 106-136) - _reboot_setup() function
- `build/mac/io.pcp.pmcd.plist` - launchd configuration

---

## Future Considerations

1. **Create `qa/CLAUDE.md`** with QA-specific context for AI assistants
2. **Add `darwin` or `macos` test group** for platform-specific tests
3. **Implement Darwin package checking** in `qa/admin/list-packages`
4. **Fold macOS into main `qa.yml`** once confidence is established

### TODO: Audit Existing QA Tests for Mock Library Usage

**Context**: We've added `_add_lib_path()` helper in `qa/common.rc` to handle platform-specific library path setup (DYLD_LIBRARY_PATH on Darwin, LD_LIBRARY_PATH on Linux). Tests 744, 745, and 1996 were updated to use this helper.

**Task**: Search through all QA tests to find others that directly set LD_LIBRARY_PATH and convert them to use `_add_lib_path()` instead.

**Why**: Many tests that use mock/wrapper libraries (e.g., for NVIDIA, InfiniBand, etc.) currently only set LD_LIBRARY_PATH, which doesn't work on macOS. These tests will fail on Darwin unless they use the cross-platform helper.

**How to Search**:
```bash
cd qa
grep -l 'LD_LIBRARY_PATH.*=' [0-9]* | grep -v '.out'
```

**Pattern to Replace**:
```bash
# Old (Linux-only):
LD_LIBRARY_PATH=$here/src; export LD_LIBRARY_PATH

# New (cross-platform):
_add_lib_path $here/src
```

**Documentation Needed**: Add a section to `qa/CLAUDE.md` (when created) explaining:
- When to use `_add_lib_path()` (any time you need mock/test libraries)
- Why not to hardcode LD_LIBRARY_PATH directly
- Example from qa/744 showing the pattern

**Priority**: Medium - can be done incrementally as tests are found failing on macOS
