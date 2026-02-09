# QA Testing Infrastructure - Guide for AI Assistants

This file provides critical context about PCP's Quality Assurance (QA) testing infrastructure for AI assistants working with this codebase.

## Critical Learnings

### Internal Headers in Test Code

**CRITICAL**: QA test PMDAs (in `qa/pmdas/`) and test programs (in `qa/src/`) legitimately use INTERNAL PCP headers that are marked `NOSHIP` and are NOT installed to the system include directories.

#### The libpcp.h Conundrum

`libpcp.h` is an internal header containing private symbols like `PDU_FLAG_AUTH`, `pmDebugOptions`, etc. It is:
- Marked `NOSHIP` in `src/include/pcp/GNUmakefile`
- **NOT installed** to `/usr/include/pcp/` or `/usr/local/include/pcp/`
- **ONLY available** in the build tree and the installed testsuite directory

**Key Locations**:
- Build tree: `$(TOPDIR)/src/include/pcp/libpcp.h`
- Installed testsuite: `/var/lib/pcp/testsuite/src/libpcp.h` (note: flattened, no `pcp/` subdirectory)

#### How qa/src Handles This

Files in `qa/src/` use:
```c
#include "libpcp.h"  // Quoted include, no pcp/ prefix
```

The `qa/src/GNUmakefile` creates a symlink:
```makefile
libpcp.h: $(TOPDIR)/src/include/pcp/libpcp.h
	rm -f libpcp.h
	$(LN_S) $(TOPDIR)/src/include/pcp/libpcp.h libpcp.h
```

Then uses `-I$(TOPDIR)/src/include/pcp` (build) or `-I.` (installed) to find it.

#### How qa/pmdas Should Handle This

**DO**:
- Use `#include "libpcp.h"` (quoted, no `pcp/` prefix) to match `qa/src` pattern
- Add `-I../../src` to CFLAGS in `GNUmakefile.install` for installed testsuite builds
- Add `-I$(TOPDIR)/src/include/pcp` for source tree builds

**DON'T**:
- Use `#include <pcp/libpcp.h>` - this requires a `pcp/` subdirectory that doesn't exist after installation
- Try to remove `libpcp.h` includes from test code - they're legitimately needed
- Assume libpcp.h will be in system include directories - it won't be

#### The macOS vs Linux Difference

**Why Linux "works"**:
- On Linux, `PCP_INC_DIR=/usr/include/pcp`
- The GNUmakefile condition `ifneq "$(PCP_INC_DIR)" "/usr/include/pcp"` is FALSE
- So it uses `-I../../src` which finds the installed libpcp.h

**Why macOS initially failed**:
- On macOS, `PCP_INC_DIR=/usr/local/include/pcp` (different prefix)
- The condition is TRUE (not equal to `/usr/include/pcp`)
- So it used `-I/usr/local/include/pcp` which doesn't have libpcp.h

**The Fix**:
- Added Darwin-specific case to explicitly use `-I../../src`
- Changed include style to match `qa/src` pattern

### Test PMDAs vs Production PMDAs

**Production PMDAs** (`src/pmdas/*`):
- Use ONLY public API headers (`pmapi.h`, `pmda.h`)
- Must work when installed system-wide
- Follow strict API boundaries

**Test PMDAs** (`qa/pmdas/*`):
- CAN use internal headers (`libpcp.h`)
- Exist to test internal functionality and edge cases
- More permissive - they're test code, not production code

**Don't "clean up" test code by removing internal headers without understanding why they're there!**

## Test Infrastructure

### Directory Structure

```
qa/
├── src/              # Test programs (binaries that exercise PCP)
├── pmdas/            # Test PMDAs (for testing PMDA functionality)
│   ├── dynamic/      # Dynamic indom test PMDA
│   ├── broken/       # Deliberately broken PMDA for error testing
│   ├── trivial/      # Minimal PMDA example
│   └── ...
├── common.rc         # Common shell functions for tests
├── common.check      # Service management functions
├── check             # Main test runner
└── [0-9]*            # Individual test scripts
```

### Build vs Install

**Two different makefile workflows**:

1. **Source tree build** (`GNUmakefile`):
   - Uses `$(TOPDIR)` relative paths
   - Include: `-I$(TOPDIR)/src/include`
   - Links against build tree libraries

2. **Installed testsuite** (`GNUmakefile.install`):
   - Installed to `/var/lib/pcp/testsuite/`
   - Gets renamed to just `GNUmakefile` when installed
   - Include: `-I../../src` (for internal headers), `-I$(PCP_INC_DIR)/..` (for public headers)
   - Links against installed libraries

**Critical**: The `GNUmakefile.install` file is what gets used when QA tests run `make setup`!

### Platform-Specific Paths

**Linux**:
- Prefix: `/usr`
- Includes: `/usr/include/pcp`
- Libraries: `/usr/lib` or `/usr/lib64`

**macOS**:
- Prefix: `/usr/local` (or `/opt/homebrew` for Homebrew)
- Includes: `/usr/local/include/pcp`
- Libraries: `/usr/local/lib`
- Why: `/usr` is reserved for Apple system software; third-party software uses `/usr/local`

**BSD**:
- Similar to macOS, typically uses `/usr/local`

## Common Pitfalls

### 1. Assuming Linux-only Behavior

**Problem**: Code that works on Linux might assume `/usr/include/pcp` paths.

**Solution**: Use `$(PCP_INC_DIR)` from `pcp.conf` and handle different prefixes properly.

### 2. Removing "Unnecessary" Includes

**Problem**: Seeing `#include <pcp/libpcp.h>` and thinking "this should use public APIs only".

**Solution**: Test code is ALLOWED to use internal headers. Check the Makefile before removing includes.

### 3. Not Handling Angle vs Quote Includes

**Problem**: Using `<pcp/libpcp.h>` style when libpcp.h is installed flat (no `pcp/` subdirectory).

**Solution**: Use `"libpcp.h"` style for internal headers in test code.

### 4. Forgetting About GNUmakefile.install

**Problem**: Fixing `GNUmakefile` but not `GNUmakefile.install`, which is what actually runs in the testsuite.

**Solution**: When fixing build issues, check BOTH makefiles. The `.install` version is what matters for QA.

## Testing Locally

### Running QA Tests

```bash
# Full sanity suite
cd /var/lib/pcp/testsuite
sudo -u pcpqa ./check -g sanity

# Single test
sudo -u pcpqa ./check 123

# Test range
sudo -u pcpqa ./check 100-200

# Specific group
sudo -u pcpqa ./check -g pmda
```

### Debugging Test PMDA Build Issues

```bash
# Check what's actually installed
ls -la /var/lib/pcp/testsuite/pmdas/dynamic/

# Try building manually
cd /var/lib/pcp/testsuite/pmdas/dynamic
sudo -u pcpqa make -n setup    # Dry run to see commands
sudo -u pcpqa make setup        # Actual build

# Check include paths
grep CFLAGS GNUmakefile
```

### Checking Header Locations

```bash
. /etc/pcp.conf
echo "PCP_INC_DIR=$PCP_INC_DIR"

# Check for libpcp.h in various locations
ls -la $PCP_INC_DIR/libpcp.h                    # Won't exist (NOSHIP)
ls -la $PCP_INC_DIR/../libpcp.h                 # Won't exist
ls -la /var/lib/pcp/testsuite/src/libpcp.h      # Should exist
```

## Resources

- Main QA README: `qa/README`
- Service management: `qa/common.check`
- Test helpers: `qa/common.rc`
- macOS QA plan: `build/mac/plans/MACOS_QA_IMPLEMENTATION_PLAN.md`
