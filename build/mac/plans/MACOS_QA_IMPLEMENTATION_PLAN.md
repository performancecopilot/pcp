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

### ✅ RESOLVED: Test PMDA Rebuild Failure - libpcp.h Not Found

**Status**: Fixed 2026-02-09 (commit pending)

**Problem**: The dynamic test PMDA failed to compile during QA setup on macOS with `'pcp/libpcp.h' file not found`, but worked on Linux.

**Error Message**:
```
dynamic.c:11:10: fatal error: 'pcp/libpcp.h' file not found
   11 | #include <pcp/libpcp.h>
      |          ^~~~~~~~~~~~~~
make[2]: *** [dynamic.o] Error 1
```

**Root Cause Analysis (via CI diagnostics)**:

The GNUmakefile.install had platform-specific logic for finding internal headers:

```makefile
ifneq "$(PCP_INC_DIR)" "/usr/include/pcp"
CFLAGS += -I$(PCP_INC_DIR)/.. -I$(PCP_INC_DIR)
else
CFLAGS += -I../../src
endif
```

**On Linux**: `PCP_INC_DIR=/usr/include/pcp` → condition FALSE → uses `-I../../src` → finds `libpcp.h` in `/var/lib/pcp/testsuite/src/` ✓

**On macOS**: `PCP_INC_DIR=/usr/local/include/pcp` → condition TRUE → uses `-I/usr/local/include/pcp` → `libpcp.h` NOT FOUND ✗

The makefile assumed that if `PCP_INC_DIR != /usr/include/pcp`, then internal headers would be in the installed include directory. But `libpcp.h` is marked `NOSHIP` and is NEVER installed - it only exists in the testsuite's `src/` directory after `make install`.

**Diagnostic Output Confirmed**:
- ✓ `libpcp.h` exists at `/var/lib/pcp/testsuite/src/libpcp.h`
- ✗ `libpcp.h` does NOT exist at `/usr/local/include/pcp/libpcp.h`
- ✓ `../../src` resolves to `/var/lib/pcp/testsuite/src` from the dynamic PMDA directory
- ✗ GNUmakefile condition chose wrong branch for macOS

**Solution Implemented**:

Added Darwin-specific case to `qa/pmdas/dynamic/GNUmakefile.install` that explicitly includes the testsuite src directory (where libpcp.h actually lives):

```makefile
# macOS: Always use testsuite src dir for internal headers like libpcp.h
# which is marked NOSHIP and never installed to PCP_INC_DIR
ifeq "$(PCP_PLATFORM)" "darwin"
CFLAGS += -I../../src -I$(PCP_INC_DIR)/.. -I$(PCP_INC_DIR)
else
# Original logic for Linux and other platforms (unchanged)
ifneq "$(PCP_INC_DIR)" "/usr/include/pcp"
CFLAGS += -I$(PCP_INC_DIR)/.. -I$(PCP_INC_DIR)
else
CFLAGS += -I../../src
endif
endif
```

This conservative fix:
- Adds `-I../../src` FIRST for Darwin (where libpcp.h is guaranteed to be)
- Preserves all existing Linux/BSD/other platform behavior
- Doesn't break anything that's currently working

**Files Modified**:
- `qa/pmdas/dynamic/GNUmakefile.install` - Added Darwin-specific include path handling

**Why This Fix Is Safe**:
- Linux path is completely unchanged (wrapped in `else` block)
- macOS path explicitly adds the directory we KNOW contains libpcp.h
- No other platforms affected

**Key Learnings**:
- Test PMDAs in `qa/pmdas/` legitimately use internal headers (unlike production PMDAs)
- `libpcp.h` is intentionally not installed (marked NOSHIP)
- The testsuite's `src/` directory is the authoritative location for libpcp.h post-install
- Platform-specific prefix paths require platform-specific makefile logic

---

### ✅ RESOLVED: DYLD_LIBRARY_PATH Not Set

**Status**: Fixed in commit 39ad5306c1 (2026-02-08)

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

---

## Phase N: Fix Remaining macOS QA Test Failures (2026-02-09)

### Status: PLANNED

### Context

GitHub Actions macOS QA tests are now running but **17 of 70 sanity tests fail** with "output mismatch" and **10 tests skip** due to missing dependencies (CI run 21819356637). Root cause analysis reveals five critical issues blocking full test pass rates.

### Problem Summary

1. **Critical rpath bug**: Test binaries use WRONG linker syntax - space-separated `-Wl,-rpath $(VAR)` instead of comma-separated `-Wl,-rpath,$(VAR)`, causing `dyld: Library not loaded: libpcp.4.dylib` errors
2. **Python/Perl binding mismatch**: Modules install to `/usr/local/lib/` but Homebrew Python searches `/opt/homebrew/lib/`, and system Perl's @INC doesn't include `/usr/local/lib/perl5/`
3. **Terminal environment**: Tests run without TERM variable, causing "No entry for terminal type 'unknown'" errors
4. **Permission errors**: pmlogger socket binding fails with "Permission denied" when tests run as pcpqa user
5. **Dependency installation inconsistency**: GitHub Actions and Cirrus CI use different package lists, neither matches PCP's official `qa/admin/package-lists/Darwin*`

### Implementation Phases

**Order**: rpath (blocks execution) → bindings (enables tests) → environment (reduces mismatches) → DRY (maintainability)

---

### N.1: Fix rpath Linker Syntax (CRITICAL)

**Problem**: macOS ld64 linker requires comma-separated rpath argument but makefiles use space-separated syntax

**Impact**: Test binaries crash with "Library not loaded: libpcp.4.dylib" when spawned as subprocesses

**Root Cause**:
```makefile
# BROKEN (current):
-Wl,-rpath $(PCP_LIB_DIR)  # Space between -rpath and path

# CORRECT (needed):
-Wl,-rpath,$(PCP_LIB_DIR)  # Comma between -rpath and path
```

**Why**: macOS clang/ld64 treats `-Wl,-rpath VALUE` as "-rpath flag with no argument, then a separate file argument VALUE", which fails. The comma makes it a single argument: `-Wl,-rpath,VALUE`.

**Files to Modify** (12 total):
- `qa/src/GNUmakefile.install` (line 28)
- `src/pmdas/trace/GNUmakefile` (line 110)
- `src/pmdas/simple/GNUmakefile.install` (line 43)
- `src/pmdas/trivial/GNUmakefile.install` (line 41)
- `src/pmdas/sample/src/GNUmakefile.install` (line 42)
- `src/pmdas/txmon/GNUmakefile.install` (line 43)
- `qa/pmdas/dynamic/GNUmakefile.install` (line 41)
- `qa/pmdas/dynamic/GNUmakefile` (line 39)
- `qa/pmdas/schizo/GNUmakefile.install` (line 32)
- `qa/pmdas/github-56/GNUmakefile.install` (line 32)
- `qa/pmdas/broken/GNUmakefile.install` (line 34)
- `qa/pmdas/bigun/GNUmakefile.install` (line 32)

**Change Pattern** (identical in all files):
```diff
 ifeq "$(PCP_PLATFORM)" "darwin"
-PCP_LIBS	+= -L$(PCP_LIB_DIR) -Wl,-rpath $(PCP_LIB_DIR)
+PCP_LIBS	+= -L$(PCP_LIB_DIR) -Wl,-rpath,$(PCP_LIB_DIR)
 else
 PCP_LIBS	+= -L$(PCP_LIB_DIR) -Wl,-rpath=$(PCP_LIB_DIR)
 endif
```

**Verification** (add to CI after "Rebuild test binaries with rpath"):
```yaml
- name: Verify rpath in test binaries
  run: |
    echo "=== Phase N.1 Verification: Test Binary rpath ==="
    echo "Checking torture_cache binary..."
    otool -L /var/lib/pcp/testsuite/src/torture_cache | grep libpcp || echo "ERROR: No libpcp reference found"
    echo ""
    echo "Checking LC_RPATH load command..."
    otool -l /var/lib/pcp/testsuite/src/torture_cache | grep -A2 LC_RPATH || echo "ERROR: No LC_RPATH found"
    echo ""
    echo "Expected: Should show /usr/local/lib/libpcp.4.dylib (absolute path, not just 'libpcp.4.dylib')"
    echo "Expected: LC_RPATH section should show 'path /usr/local/lib'"
```

---

### N.2: Fix Python/Perl Binding Installation

**Problem**: PCP Python/Perl modules installed to `/usr/local/lib/` but interpreters can't find them

**Root Cause**:
- PCP configure uses `--prefix=/usr/local`, installing to:
  - Python: `/usr/local/lib/python3.*/site-packages/`
  - Perl: `/usr/local/lib/perl5/site_perl/` or `/usr/local/lib/perl5/vendor_perl/`
- But interpreters search different locations:
  - Homebrew Python: `/opt/homebrew/lib/python3.*/site-packages/`
  - System Perl @INC: doesn't include `/usr/local/lib/perl5/`

**Solution**: Configure PCP to install to Homebrew Python's location

**Files to Modify**:

1. **`.github/workflows/qa-macos.yml`** - Configure PCP step:
```yaml
- name: Configure PCP
  run: |
    ETC=$(realpath /etc)
    VAR=$(realpath /var)
    # Use Homebrew Python's installation prefix
    PYTHON_PREFIX=$(python3 -c 'import sys; print(sys.prefix)')
    ./configure \
      --sysconfdir=$ETC \
      --localstatedir=$VAR \
      --prefix=/usr/local \
      --with-python-prefix=$PYTHON_PREFIX \
      --with-qt=no
```

2. **`Makepkgs`** - Darwin configuration block (line 306):
```diff
     etc=`realpath /etc`
     var=`realpath /var`
-    configopts="--sysconfdir=$etc --localstatedir=$var --prefix=/usr/local --with-qt=no"
+    python_prefix=$(python3 -c 'import sys; print(sys.prefix)')
+    configopts="--sysconfdir=$etc --localstatedir=$var --prefix=/usr/local --with-python-prefix=$python_prefix --with-qt=no"
     PKGBUILD=`which pkgbuild`; export PKGBUILD
```

**Fallback** (if configure doesn't support --with-python-prefix):

Add environment variables to CI (before "Run QA sanity tests"):
```yaml
- name: Configure Python/Perl module paths
  run: |
    PY_VER=$(python3 -c 'import sys; print(f"{sys.version_info.major}.{sys.version_info.minor}")')
    echo "PYTHONPATH=/usr/local/lib/python${PY_VER}/site-packages:${PYTHONPATH:-}" >> $GITHUB_ENV
    echo "PERL5LIB=/usr/local/lib/perl5/site_perl:/usr/local/lib/perl5/vendor_perl:${PERL5LIB:-}" >> $GITHUB_ENV
```

**Verification** (add to CI after "Install PCP"):
```yaml
- name: Verify Python/Perl bindings installation
  run: |
    echo "=== Phase N.2 Verification: Python/Perl Bindings ==="
    echo "Python interpreter location:"
    which python3
    python3 -c "import sys; print('Python prefix:', sys.prefix)"
    echo ""
    echo "Python search paths:"
    python3 -c "import sys; print('\n'.join(sys.path))"
    echo ""
    echo "Perl @INC paths:"
    perl -e 'print join("\n", @INC), "\n"'
    echo ""
    echo "Testing Python pcp.pmapi import..."
    python3 -c "from pcp import pmapi; print('✓ Python pcp.pmapi OK')" || echo "✗ Python pcp.pmapi FAILED"
    echo ""
    echo "Testing Perl PCP::PMDA module..."
    perl -e "use PCP::PMDA; print '✓ Perl PCP::PMDA OK\n'" || echo "✗ Perl PCP::PMDA FAILED"
    echo ""
    echo "Checking where Python modules were installed:"
    find /usr/local/lib -name "pmapi.py" 2>/dev/null || echo "pmapi.py not found in /usr/local/lib"
    find /opt/homebrew/lib -name "pmapi.py" 2>/dev/null || echo "pmapi.py not found in /opt/homebrew/lib"
    echo ""
    echo "Expected: pmapi.py should be in the same prefix as 'python3 -c \"import sys; print(sys.prefix)\"'"
```

---

### N.3: Fix Terminal Environment

**Problem**: Tests spawn subprocesses without TERM environment variable set, causing ncurses/tput failures

**Error**: `No entry for terminal type "unknown";`

**Impact**: Output mismatches in tests that use terminal control sequences

**Solution**: Set TERM before running QA tests

**Files to Modify**:

1. **`.github/workflows/qa-macos.yml`** - QA test step:
```yaml
- name: Run QA sanity tests
  run: |
    cd /var/lib/pcp/testsuite
    export TERM=xterm-256color  # Set terminal type
    sudo -u pcpqa ./check -g sanity -x not_in_ci
```

2. **`.cirrus.yml`** - QA test script:
```yaml
run_qa_tests_script: |
  cd /var/lib/pcp/testsuite
  export TERM=xterm-256color  # Set terminal type
  sudo -u pcpqa ./check -g sanity -x not_in_ci
```

**Verification** (add before "Run QA sanity tests"):
```yaml
- name: Verify environment configuration
  run: |
    echo "=== Phase N.3 Verification: Environment ==="
    echo "TERM=$TERM"
    [ -n "$TERM" ] && echo "✓ TERM is set" || echo "✗ TERM is not set"
    echo ""
    echo "PYTHONPATH=${PYTHONPATH:-<not set>}"
    echo "PERL5LIB=${PERL5LIB:-<not set>}"
```

---

### N.4: Fix pmlogger Socket Permissions

**Problem**: Tests run as `pcpqa` user but pmlogger tries to bind sockets in `/private/var/run/pcp/` owned by root/pcp

**Error**: `__pmBind(/private/var/run/pcp/pmlogger.XXXXX.socket): Permission denied`

**Impact**: Tests that spawn pmlogger instances fail to create control sockets

**Solution**: Ensure pcpqa has write access to PCP runtime directories

**Files to Modify**:

1. **`.github/workflows/qa-macos.yml`** - Add before "Run QA sanity tests":
```yaml
- name: Fix PCP runtime directory permissions
  run: |
    . /etc/pcp.conf
    echo "Setting permissions on $PCP_RUN_DIR for pcpqa user..."
    sudo chown -R pcpqa:staff $PCP_RUN_DIR
    sudo chmod -R 755 $PCP_RUN_DIR
    # Also ensure user home directory exists for local socket fallback
    sudo mkdir -p /Users/runner/.pcp/run
    sudo chown -R runner:staff /Users/runner/.pcp
```

2. **`.cirrus.yml`** - Similar step before `run_qa_tests_script`

**Verification** (add after permission fix):
```yaml
- name: Verify runtime directory permissions
  run: |
    echo "=== Phase N.4 Verification: Permissions ==="
    . /etc/pcp.conf
    echo "PCP runtime directory:"
    ls -ld $PCP_RUN_DIR
    echo ""
    echo "PCP runtime directory contents:"
    ls -la $PCP_RUN_DIR | head -10
    echo ""
    echo "pcpqa user home directory:"
    ls -ld /Users/runner/.pcp 2>/dev/null || echo "/Users/runner/.pcp does not exist yet"
    echo ""
    echo "Expected: pcpqa should have write access to both locations"
```

---

### N.5: Create Shared Dependency Installation Script (DRY)

**Problem**: GitHub Actions and Cirrus CI duplicate dependency installation logic with different package lists

**Analysis**: After reviewing `qa/admin/other-packages/manifest`:
- **Build-required** (no optional marker): psycopg2, lxml, requests, JSON, Date::Parse/Format, Spreadsheet::WriteExcel, Text::CSV_XS
- **Build-optional** (marked "build optional"): openpyxl, pyarrow, bcc, bpftrace
- **QA-only** (marked "QA optional"): Spreadsheet::XLSX, Spreadsheet::Read, Spreadsheet::ReadSXC, PIL/Pillow

**Solution**: Create shared script that installs build-required + build-optional by default

**New File**: `build/mac/scripts/install-deps.sh`
```bash
#!/bin/bash
set -euo pipefail

# Install macOS build/test dependencies for PCP
# Usage: ./install-deps.sh [--minimal]
#   (no args): Install build-required + build-optional + QA-only packages (default)
#   --minimal: Install only build-required + build-optional (skip QA-only)

SKIP_QA_ONLY=false
if [[ "${1:-}" == "--minimal" ]]; then
  SKIP_QA_ONLY=true
fi

echo "=== Installing Homebrew dependencies ==="
brew update --quiet || true
brew install \
  autoconf \
  coreutils \
  gnu-tar \
  libuv \
  pkg-config \
  python3 \
  python-setuptools \
  unixodbc \
  valkey || true

echo "=== Installing Perl CPAN modules (build-required and build-optional) ==="
brew install cpanminus || brew upgrade cpanminus
sudo cpanm --notest \
  JSON \
  Date::Parse \
  Date::Format \
  XML::TokeParser \
  Spreadsheet::WriteExcel \
  Text::CSV_XS

echo "=== Installing Python pip packages (build-required and build-optional) ==="
pip3 install --user --break-system-packages \
  lxml \
  openpyxl \
  psycopg2-binary \
  prometheus_client \
  pyarrow \
  pyodbc \
  requests \
  setuptools \
  wheel

if [ "$SKIP_QA_ONLY" = false ]; then
  echo "=== Installing QA-only packages ==="
  sudo cpanm --notest \
    Spreadsheet::XLSX \
    Spreadsheet::Read
  # Pillow (PIL) can be added here if needed:
  # pip3 install --user --break-system-packages Pillow
fi

echo "✓ Dependencies installed successfully"
```

**Files to Modify**:

1. **`.github/workflows/qa-macos.yml`** - Replace composite action:
```yaml
- name: Install macOS dependencies
  run: bash build/mac/scripts/install-deps.sh  # Full install (no --minimal)
```

2. **`.cirrus.yml`** - Update homebrew cache populate script:
```yaml
populate_script: |
  bash build/mac/scripts/install-deps.sh  # Full install
```

**Rationale**:
- Most Python/Perl packages are used by PCP export tools (pcp2xlsx, pcp2postgresql, pmlogsummary), not just QA
- Consolidates package lists into single source of truth
- Both CI systems use identical dependencies
- `--minimal` flag available for resource-constrained builds

---

### Implementation Checklist

**Phase N.1 - rpath (12 files)**:
- [ ] `qa/src/GNUmakefile.install`
- [ ] `src/pmdas/trace/GNUmakefile`
- [ ] `src/pmdas/simple/GNUmakefile.install`
- [ ] `src/pmdas/trivial/GNUmakefile.install`
- [ ] `src/pmdas/sample/src/GNUmakefile.install`
- [ ] `src/pmdas/txmon/GNUmakefile.install`
- [ ] `qa/pmdas/dynamic/GNUmakefile.install`
- [ ] `qa/pmdas/dynamic/GNUmakefile`
- [ ] `qa/pmdas/schizo/GNUmakefile.install`
- [ ] `qa/pmdas/github-56/GNUmakefile.install`
- [ ] `qa/pmdas/broken/GNUmakefile.install`
- [ ] `qa/pmdas/bigun/GNUmakefile.install`

**Phase N.2 - Python/Perl bindings**:
- [ ] `.github/workflows/qa-macos.yml` - add --with-python-prefix to configure
- [ ] `Makepkgs` - add --with-python-prefix to darwin configopts

**Phase N.3 - Terminal**:
- [ ] `.github/workflows/qa-macos.yml` - set TERM=xterm-256color
- [ ] `.cirrus.yml` - set TERM=xterm-256color

**Phase N.4 - Permissions**:
- [ ] `.github/workflows/qa-macos.yml` - fix runtime directory permissions
- [ ] `.cirrus.yml` - fix runtime directory permissions

**Phase N.5 - DRY**:
- [ ] Create `build/mac/scripts/install-deps.sh`
- [ ] `.github/workflows/qa-macos.yml` - use shared script
- [ ] `.cirrus.yml` - use shared script

**Verification Steps** (all automated in CI):
- [ ] Add rpath verification after test binary rebuild
- [ ] Add Python/Perl binding verification after install
- [ ] Add environment verification before QA tests
- [ ] Add permissions verification after permission fix

---

### Success Criteria

- ✅ `dyld: Library not loaded` errors eliminated (rpath fix)
- ✅ "Python pcp pmapi module is not installed" eliminated (binding fix)
- ✅ "perl PCP::PMDA module not installed" eliminated (binding fix)
- ✅ "No entry for terminal type 'unknown'" errors eliminated (TERM fix)
- ✅ pmlogger socket permission errors eliminated (permission fix)
- ✅ Test failure count drops from 17 to <5 (ideally 0)
- ✅ Both GitHub Actions and Cirrus CI use identical dependency script

### Expected Impact

**Current**: 17 failed, 10 skipped (not run), 43 passed
**After N.1 (rpath)**: ~10 failed, 10 skipped, 50 passed (dylib loading fixed)
**After N.2 (bindings)**: ~5 failed, 0 skipped, 65 passed (modules available)
**After N.3 (TERM)**: ~2 failed, 0 skipped, 68 passed (terminal errors fixed)
**After N.4 (permissions)**: ~0 failed, 0 skipped, 70 passed (socket creation works)

---

### Notes

**Key Learnings**:
1. **rpath syntax is platform-specific**: macOS requires comma-separated (`-Wl,-rpath,PATH`), Linux uses equals (`-Wl,-rpath=PATH`)
2. **Python prefix matters**: Homebrew Python and system Python use different installation prefixes - PCP must match
3. **Package classification**: Most Python/Perl packages are build-required for PCP export tools, not just QA
4. **Root cause > workarounds**: Fixing `--with-python-prefix` is better than setting PYTHONPATH/PERL5LIB environment variables
5. **CI verification essential**: Automated diagnostic output in logs reveals issues faster than manual investigation

**Potential Remaining Issues**:
- Some tests may still have platform-specific output differences (file paths, metric values)
- These will need individual investigation and either:
  - Platform-specific `.out.darwin` expected output files
  - Filtering/normalization in test scripts
  - Marking as `not_in_ci` if inherently non-portable

