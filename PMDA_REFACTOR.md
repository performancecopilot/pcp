 # Step 4.2: Refactor pmda.c Legacy Code

## Implementation Status

### ✅ Part 1: Extract Metric Table (COMPLETED)

**Commits:**
- `20656d75ba` - Extract metrictab to metrics.c/metrics.h
- `9f4ec0952e` - Fix build: move CLUSTER enum to darwin.h and add headers to metrics.c
- `a21fb52a1d` - Update PMDA_REFACTOR.md: mark Part 1 complete

**Results:**
- ✅ Build succeeds
- ✅ Unit tests pass (dbpmda)
- ✅ Integration tests pass (pminfo/pmval)
- ✅ Code review: READY TO MERGE
- ✅ pmda.c reduced from 1556 to 768 lines (788 lines removed, 50.6% reduction)

**Files Created:**
- `src/pmdas/darwin/metrics.c` (826 lines) - Metric table definition
- `src/pmdas/darwin/metrics.h` (26 lines) - External declarations

**Files Modified:**
- `src/pmdas/darwin/pmda.c` - Removed metrictab array, updated pmdaInit call
- `src/pmdas/darwin/darwin.h` - Added CLUSTER_* enum for cross-module visibility
- `src/pmdas/darwin/GNUmakefile` - Added metrics.c and metrics.h to build

**QA Verification:** Passed in isolated Cirrus VM environment via macos-darwin-pmda-qa agent

---

### ✅ Phase 2.1: Extract vmstat (CLUSTER_VMSTAT) (COMPLETED)

**Commits:**
- `f46c9344db` - Extract vmstat cluster to separate module
- `23111c529b` - Fix vmstat code style issues

**Results:**
- ✅ Build succeeds
- ✅ Unit tests pass (dbpmda)
- ✅ Integration tests pass (pminfo/pmval)
- ✅ Code review: PASSED (style issues corrected)
- ✅ Memory and swap metrics fully functional

**Files Created:**
- `src/pmdas/darwin/vmstat.c` (114 lines) - VM statistics refresh and fetch functions
- `src/pmdas/darwin/vmstat.h` (27 lines) - Type definitions and function declarations

**Files Modified:**
- `src/pmdas/darwin/pmda.c` - Removed fetch_vmstat() function, added vmstat.h include, updated dispatch
- `src/pmdas/darwin/kernel.c` - Moved refresh_vmstat() and refresh_swap() to vmstat.c
- `src/pmdas/darwin/GNUmakefile` - Added vmstat.c and vmstat.h to build

**QA Verification:** Passed in isolated Cirrus VM environment via macos-darwin-pmda-qa agent

---

### ✅ Phase 2.2: Extract filesys (CLUSTER_FILESYS) (COMPLETED)

**Commits:**
- `6e49b5fc54` - Extract filesys cluster to separate module

**Results:**
- ✅ Build succeeds
- ✅ Unit tests pass (dbpmda)
- ✅ Integration tests pass (pminfo/pmval)
- ✅ Code review: APPROVED FOR MERGE
- ✅ Filesystem metrics fully functional

**Files Created:**
- `src/pmdas/darwin/filesys.c` (107 lines) - Filesystem refresh and fetch functions
- `src/pmdas/darwin/filesys.h` (21 lines) - Type definitions and function declarations

**Files Modified:**
- `src/pmdas/darwin/pmda.c` - Removed fetch_filesys() function (58 lines), added filesys.h include, updated dispatch
- `src/pmdas/darwin/kernel.c` - Moved refresh_filesys() to filesys.c
- `src/pmdas/darwin/GNUmakefile` - Added filesys.c and filesys.h to build

**QA Verification:** Passed in isolated Cirrus VM environment via macos-darwin-pmda-qa agent

---

### ✅ Phase 2.3: Extract cpu (CLUSTER_CPU) (COMPLETED)

**Commits:**
- `8b0bc27e58` - Extract cpu cluster to separate module
- `90f530d4a2` - Add missing extern declaration for refresh_cpus

**Results:**
- ✅ Build succeeds
- ✅ Unit tests pass (dbpmda)
- ✅ Integration tests pass (pminfo/pmval)
- ✅ Code review: PASSED after fix
- ✅ Per-CPU metrics fully functional

**Files Created:**
- `src/pmdas/darwin/cpu.c` (109 lines) - CPU refresh and fetch functions
- `src/pmdas/darwin/cpu.h` (26 lines) - Type definitions and function declarations

**Files Modified:**
- `src/pmdas/darwin/pmda.c` - Removed fetch_cpu() function (32 lines), added cpu.h include, added extern declaration for refresh_cpus()
- `src/pmdas/darwin/kernel.c` - Moved refresh_cpus() to cpu.c (46 lines removed)
- `src/pmdas/darwin/GNUmakefile` - Added cpu.c and cpu.h to build

**QA Verification:** Passed in isolated Cirrus VM environment via macos-darwin-pmda-qa agent

---

### ✅ Phase 2.4: Extract nfs (CLUSTER_NFS) (COMPLETED)

**Commits:**
- `5b1fcc7226` - Extract nfs cluster to separate module
- `4f9cd763cd` - Fix NFS module compilation issues

**Results:**
- ✅ Build succeeds
- ✅ Unit tests pass (dbpmda)
- ✅ Integration tests pass (pminfo/pmval)
- ✅ Code review: APPROVED FOR MERGE
- ✅ NFS metrics fully functional

**Files Created:**
- `src/pmdas/darwin/nfs.c` (85 lines) - NFS refresh and fetch functions
- `src/pmdas/darwin/nfs.h` (26 lines) - Type definitions and function declarations

**Files Modified:**
- `src/pmdas/darwin/pmda.c` - Removed fetch_nfs() function (30 lines), added nfs.h include, removed extern declaration for refresh_nfs()
- `src/pmdas/darwin/network.c` - Moved refresh_nfs() to nfs.c (23 lines removed)
- `src/pmdas/darwin/darwin.h` - Added NFS3_RPC_COUNT constant definition
- `src/pmdas/darwin/GNUmakefile` - Added nfs.c and nfs.h to build

**QA Verification:** Passed in isolated Cirrus VM environment via macos-darwin-pmda-qa agent

---

### ✅ Phase 2.5: Extract Low-Priority Subsystems (COMPLETED)

**Commits:**
- `b1ff808252` - Extract loadavg cluster to separate module
- `9e4f79b2b6` - Extract cpuload cluster to separate module
- `e563b70b6e` - Extract uname cluster to separate module

**Results:**
- ✅ Build succeeds
- ✅ Unit tests pass (dbpmda)
- ✅ Integration tests pass (pminfo/pmval)
- ✅ Code review: APPROVED FOR MERGE
- ✅ All metrics fully functional

**Files Created:**
- `src/pmdas/darwin/loadavg.c` (61 lines) - Load average refresh and fetch functions
- `src/pmdas/darwin/loadavg.h` (30 lines) - Type definitions and function declarations
- `src/pmdas/darwin/cpuload.c` (64 lines) - CPU load refresh and fetch functions
- `src/pmdas/darwin/cpuload.h` (32 lines) - Type definitions and function declarations
- `src/pmdas/darwin/uname.c` (57 lines) - Uname/version refresh and fetch functions
- `src/pmdas/darwin/uname.h` (32 lines) - Type definitions and function declarations

**Files Modified:**
- `src/pmdas/darwin/pmda.c` - Removed fetch_loadavg() (18 lines), fetch_cpuload() (24 lines), fetch_uname() (23 lines); added includes; removed extern declarations (65 lines removed total)
- `src/pmdas/darwin/kernel.c` - Moved refresh_loadavg() (13 lines), refresh_cpuload() (8 lines), refresh_uname() (4 lines) to respective modules (25 lines removed)
- `src/pmdas/darwin/GNUmakefile` - Added loadavg.c/h, cpuload.c/h, uname.c/h to build

**QA Verification:** All three extractions passed in isolated Cirrus VM environment via macos-darwin-pmda-qa agent

**Decision Notes:**
- Originally planned as DEFER (optional), but extracted for consistency with established modular pattern
- All subsystems now follow the same architectural pattern
- pmda.c is now purely coordination logic with no embedded fetch functions

---

## Overview

Refactor pmda.c to improve code organization and maintainability by:
1. **Extract metrictab[] to separate file** (reduces pmda.c by ~784 lines of metric definitions)
2. **Extract embedded fetch functions** to dedicated subsystem files following established modular pattern

**Goal**: Transform pmda.c into a clean coordination layer while maintaining zero functional changes.

**Constraint**: This is PURE code reorganization - all existing tests must continue to pass.

---

## Part 1: Extract Metric Table (metrictab)

### Current Problem

The `static pmdaMetric metrictab[]` array spans **lines 182-966 (784 lines)** and obscures the actual logic in pmda.c. This massive array of metric definitions makes the file harder to navigate and understand.

### Solution: Follow Solaris/AIX Pattern

Both Solaris and AIX PMDAs extract their metric tables to a separate `data.c` file. We'll do the same for darwin.

### Implementation

**Files to Create:**

1. **src/pmdas/darwin/metrics.h**:
   ```c
   /*
    * Darwin PMDA metric table declarations
    * Copyright (c) 2026 Red Hat.
    * ... GPL header ...
    */

   #ifndef METRICS_H
   #define METRICS_H

   #include "pmapi.h"
   #include "pmda.h"

   extern pmdaMetric metrictab[];
   extern int metrictab_sz;

   #endif /* METRICS_H */
   ```

2. **src/pmdas/darwin/metrics.c**:
   ```c
   /*
    * Darwin PMDA metric table
    * Copyright (c) 2026 Red Hat.
    * ... GPL header ...
    */

   #include "pmapi.h"
   #include "pmda.h"
   #include "darwin.h"
   #include "metrics.h"

   pmdaMetric metrictab[] = {
       /* ... entire metrictab array moved here ... */
   };

   int metrictab_sz = sizeof(metrictab) / sizeof(metrictab[0]);
   ```

**Changes to pmda.c:**
- Remove `static pmdaMetric metrictab[]` (lines 182-966)
- Add `#include "metrics.h"` near top
- Remove local `metrictab_sz` calculation
- Use `extern` declarations via metrics.h

**Changes to GNUmakefile:**
- Add to CFILES: `metrics.c`
- Add to HFILES: `metrics.h`

**Benefits:**
- pmda.c reduced by ~784 lines (54% smaller!)
- Metric definitions logically separated
- Easier to navigate pmda.c for actual logic
- Follows established PCP pattern (Solaris/AIX)

**Testing:**
- All existing tests should pass (no functional change)
- Verify with `macos-darwin-pmda-qa` agent

---

## Part 2: Extract Embedded Fetch Functions

### Current State Analysis

#### Embedded Fetch Functions in pmda.c

| Function | Lines | Cluster | Complexity | Priority |
|----------|-------|---------|------------|----------|
| `fetch_vmstat()` | 60 | CLUSTER_VMSTAT (1) | High - Memory/swap logic, unit conversions | **HIGH** |
| `fetch_filesys()` | 57 | CLUSTER_FILESYS (5) | High - Filesystem calculations, instance domain | **HIGH** |
| `fetch_cpu()` | 32 | CLUSTER_CPU (8) | Medium - Per-CPU state, LOAD_SCALE conversions | **MEDIUM** |
| `fetch_nfs()` | 30 | CLUSTER_NFS (11) | Medium - NFS RPC aggregation | **MEDIUM** |
| `fetch_loadavg()` | 18 | CLUSTER_LOADAVG (3) | Low - Simple instance lookup | **LOW** |
| `fetch_cpuload()` | 24 | CLUSTER_CPULOAD (6) | Low - Simple conversions | **LOW** |
| `fetch_uname()` | 23 | CLUSTER_KERNEL_UNAME (2) | Low - String formatting | **LOW** |

**Already Refactored**: fetch_disk(), fetch_network(), fetch_vfs(), fetch_udp(), fetch_icmp(), fetch_tcp(), fetch_tcpconn()

---

### Modular Pattern (Reference)

Based on vfs.c, tcp.c, and other refactored subsystems:

#### Pattern Structure

1. **header.h** - Type definitions + function declarations
2. **source.c** - Two functions:
   - `refresh_*()` - Fetch kernel data, populate struct
   - `fetch_*()` - Map metric items to values (uses extern to access globals)
3. **pmda.c** - Globals, dispatch, coordination:
   - Global data struct: `<type>_t mach_<subsystem>`
   - Global error flag: `int mach_<subsystem>_error`
   - Dispatch in `darwin_refresh()` - calls `refresh_*()`
   - Dispatch in `darwin_fetchCallBack()` - calls `fetch_*()`

#### Key Rules

- **Fail-fast error handling**: `if (sysctlbyname(...) == -1) return -oserror();`
- **TAB indentation** (not spaces)
- **extern declarations** in fetch functions to access pmda.c globals
- **File permissions**: 644 (rw-r--r--)

---

### Implementation Plan

#### Phase 2.1: Extract vmstat (CLUSTER_VMSTAT)

**Rationale**: Most complex embedded fetch function (60 lines), handles memory and swap metrics with unit conversions.

**Files to Create**:

1. **src/pmdas/darwin/vmstat.h**:
   ```c
   /* Type definitions - reuse existing vm_statistics64 and xsw_usage */
   typedef struct vmstats {
       struct vm_statistics64 vm;
       struct xsw_usage swap;
   } vmstats_t;

   /* Function declarations */
   extern int refresh_vmstat(vmstats_t *);
   extern int fetch_vmstat(unsigned int, unsigned int, pmAtomValue *);
   ```

2. **src/pmdas/darwin/vmstat.c**:
   - Include headers: pmapi.h, pmda.h, vmstat.h, mach headers
   - Implement `refresh_vmstat()` - Move refresh logic from kernel.c
   - Implement `fetch_vmstat()` - Move from pmda.c lines 1054-1113
   - Add extern declarations for `mach_vmstat`, `mach_vmstat_error`, `mach_swap`, `mach_swap_error`
   - Keep page_count_to_kb/mb macros or move to vmstat.h

**Changes to pmda.c**:
- Add `#include "vmstat.h"`
- Keep globals: `mach_vmstat`, `mach_vmstat_error`, `mach_swap`, `mach_swap_error`
- Remove `static inline` fetch_vmstat() function (lines 1053-1113)
- Update dispatcher (line 1299): `case CLUSTER_VMSTAT: return fetch_vmstat(item, inst, atom);`
- Keep refresh dispatch - already calls external refresh_vmstat() via kernel.c

**Changes to kernel.c**:
- Move `refresh_vmstat()` logic to vmstat.c
- Keep function signature for backwards compatibility or remove if only called from pmda.c

**Changes to GNUmakefile**:
- Add to CFILES: vmstat.c
- Add to HFILES: vmstat.h

**Testing**:
- Run existing unit tests: `test-vmstat64-upgrade.txt`, `test-memory-compression.txt`
- Run integration tests for mem.* and swap.* metrics
- Verify with macos-darwin-pmda-qa agent

---

#### Phase 2.2: Extract filesys (CLUSTER_FILESYS)

**Rationale**: Second most complex (57 lines), handles filesystem capacity/usage calculations with instance domain.

**Files to Create**:

1. **src/pmdas/darwin/filesys.h**:
   ```c
   /* reuse existing struct statfs array */
   extern int refresh_filesys(struct statfs **, int *);
   extern int fetch_filesys(unsigned int, unsigned int, pmAtomValue *);
   ```

2. **src/pmdas/darwin/filesys.c**:
   - Include headers: pmapi.h, pmda.h, filesys.h, sys/mount.h
   - Implement `refresh_filesys()` - Move from kernel.c
   - Implement `fetch_filesys()` - Move from pmda.c lines 1140-1197
   - Handle instance domain logic (indomtab[FILESYS_INDOM])
   - Add extern declarations for `mach_fs`, `mach_fs_error`, `indomtab`

**Changes to pmda.c**:
- Add `#include "filesys.h"`
- Keep globals: `mach_fs`, `mach_fs_error`
- Remove fetch_filesys() function (lines 1140-1197)
- Update dispatcher (line 1301): `case CLUSTER_FILESYS: return fetch_filesys(item, inst, atom);`

**Changes to kernel.c**:
- Move `refresh_filesys()` to filesys.c

**Changes to GNUmakefile**:
- Add filesys.c and filesys.h

**Testing**:
- Run unit tests for filesys metrics
- Run integration tests for filesys.* metrics
- Verify instance domain handling with macos-darwin-pmda-qa

---

#### Phase 2.3: Extract cpu (CLUSTER_CPU)

**Rationale**: Per-CPU state accounting with LOAD_SCALE conversions (32 lines).

**Files to Create**:

1. **src/pmdas/darwin/cpu.h**:
   ```c
   /* reuse existing processor_cpu_load_info array */
   extern int refresh_cpu(processor_cpu_load_info_data_t **, int *);
   extern int fetch_cpu(unsigned int, unsigned int, pmAtomValue *);
   ```

2. **src/pmdas/darwin/cpu.c**:
   - Implement refresh and fetch functions
   - Handle CPU instance domain (indomtab[CPU_INDOM])
   - Use LOAD_SCALE macro and mach_hertz for conversions
   - Add extern declarations for `mach_cpu`, `mach_cpu_error`, `mach_hertz`, `indomtab`

**Changes to pmda.c**:
- Add `#include "cpu.h"`
- Keep globals: `mach_cpu`, `mach_cpu_error`
- Remove fetch_cpu() function (lines 1199-1231)
- Update dispatcher (line 1303): `case CLUSTER_CPU: return fetch_cpu(item, inst, atom);`

**Changes to kernel.c**:
- Move `refresh_cpu()` to cpu.c

**Changes to GNUmakefile**:
- Add cpu.c and cpu.h

**Testing**:
- Run unit tests for kernel.percpu.cpu.* metrics
- Run integration tests
- Verify per-CPU instance handling

---

#### Phase 2.4: Extract nfs (CLUSTER_NFS)

**Rationale**: NFS RPC method aggregation with instance domain (30 lines).

**Files to Create**:

1. **src/pmdas/darwin/nfs.h**:
   ```c
   /* reuse existing nfsstats_t */
   extern int refresh_nfs(nfsstats_t *);
   extern int fetch_nfs(unsigned int, unsigned int, pmAtomValue *);
   ```

2. **src/pmdas/darwin/nfs.c**:
   - Implement refresh and fetch functions
   - Handle NFS3_RPC_COUNT aggregation
   - Handle deprecated metrics (return PM_ERR_APPVERSION)
   - Add extern declarations for `mach_nfs`, `mach_nfs_error`, `indomtab`

**Changes to pmda.c**:
- Add `#include "nfs.h"`
- Keep globals: `mach_nfs`, `mach_nfs_error`
- Remove fetch_nfs() function (lines 1233-1263)
- Update dispatcher (line 1305): `case CLUSTER_NFS: return fetch_nfs(item, inst, atom);`

**Changes to kernel.c**:
- Move `refresh_nfs()` to nfs.c

**Changes to GNUmakefile**:
- Add nfs.c and nfs.h

**Testing**:
- Run unit tests for nfs3.* metrics
- Run integration tests
- Verify NFS instance domain handling

---

#### Phase 2.5: Extract Low-Priority Subsystems (COMPLETED)

**Decision**: Extract all three (loadavg, cpuload, uname) for consistency with established modular pattern.

**Rationale**: Although originally recommended to DEFER, extracting these subsystems achieves complete modularization and ensures all fetch logic follows the same architectural pattern. This eliminates special cases and makes the codebase more maintainable.

**Implementation**: Extract loadavg (CLUSTER_LOADAVG), cpuload (CLUSTER_CPULOAD), and uname (CLUSTER_KERNEL_UNAME) to separate modules following the established pattern (see Phase 2.5 results above).

---

## Implementation Sequence

Execute in this order to minimize risk:

1. **Part 1**: Extract metrictab to metrics.c/metrics.h
2. **Test checkpoint**: Run full test suite
3. **Phase 2.1**: Extract vmstat (highest complexity, highest value)
4. **Test checkpoint**: Run full test suite
5. **Phase 2.2**: Extract filesys (high complexity, instance domain handling)
6. **Test checkpoint**: Run full test suite
7. **Phase 2.3**: Extract cpu (medium complexity, per-CPU instances)
8. **Test checkpoint**: Run full test suite
9. **Phase 2.4**: Extract nfs (medium complexity, NFS instances)
10. **Test checkpoint**: Run full test suite
11. **Phase 2.5**: Evaluate low-priority extractions (likely skip)

---

## Critical Files

| File | Purpose | Changes |
|------|---------|---------|
| `src/pmdas/darwin/pmda.c` | Main PMDA coordination | Remove metrictab (784 lines), remove 4 fetch functions (~179 lines), add includes |
| `src/pmdas/darwin/metrics.{c,h}` | NEW - Metric table | Create with metrictab[] and metrictab_sz |
| `src/pmdas/darwin/kernel.c` | Data refresh logic | Move refresh_vmstat, refresh_filesys, refresh_cpu, refresh_nfs |
| `src/pmdas/darwin/vmstat.{c,h}` | NEW - Memory/swap subsystem | Create from scratch |
| `src/pmdas/darwin/filesys.{c,h}` | NEW - Filesystem subsystem | Create from scratch |
| `src/pmdas/darwin/cpu.{c,h}` | NEW - CPU subsystem | Create from scratch |
| `src/pmdas/darwin/nfs.{c,h}` | NEW - NFS subsystem | Create from scratch |
| `src/pmdas/darwin/GNUmakefile` | Build configuration | Add 10 new files (5 .c + 5 .h) |

---

## Testing Strategy

### After Each Step

1. **Commit changes to git** (macos-darwin-pmda-qa requires committed code)
2. **Invoke macos-darwin-pmda-qa agent** to run isolated build + tests
3. **Verify**:
   - Build succeeds
   - Unit tests pass (dbpmda)
   - Integration tests pass (pminfo)
   - No regressions in other subsystems
4. **Analyze failures** - fix before proceeding to next step
5. **Run pcp-code-reviewer agent** to validate:
   - Code style (TAB indentation, fail-fast error handling)
   - PCP conventions compliance
   - Documentation completeness

### Final Verification

After all extractions complete:

1. Run full unit test suite
2. Run full integration test suite
3. Compare pmda.c line count before/after (should reduce by ~960 lines: 784 metrictab + ~179 fetch functions)
4. Verify all metrics still fetchable: `pminfo -f darwin`
5. Manual testing: `pmrep :macstat`, `pmval mem.util.wired`, etc.

---

## Metrics of Success

- **pmda.c size reduction**: ~960 lines removed (metrictab + fetch functions)
- **New modular files**: 10 files created (metrics + vmstat, filesys, cpu, nfs)
- **Code organization**: All subsystems now follow consistent pattern
- **Test results**: 100% pass rate (no regressions)
- **Maintainability**: Each subsystem self-contained and independently testable
- **Readability**: pmda.c is now primarily coordination logic (~500 lines)

---

## Notes

### Why Extract metrictab First?

- Largest single chunk (784 lines = 54% of pmda.c)
- Self-contained (doesn't interact with other code)
- Low risk (simple move operation)
- Immediate dramatic improvement in readability
- Sets stage for subsequent function extractions

### Why Not Extract loadavg/cpuload/uname?

These functions are:
- **Small**: 18-24 lines each
- **Simple**: Minimal logic, straightforward value mapping
- **Cohesive**: Part of general kernel/system metrics
- **Low benefit**: Extraction overhead > value gained

**Recommendation**: Document them as "simple fetch functions" and leave in pmda.c. If future enhancements make them complex, revisit.

### Dependencies to Watch

- **vmstat**: Uses page_count_to_kb/mb macros - decide where these live (vmstat.h or keep in pmda.c)
- **filesys/cpu/nfs**: All use instance domains - ensure indomtab access works via extern
- **cpu/cpuload**: Both use LOAD_SCALE and mach_hertz - ensure consistency
- **metrics.c**: Must include all necessary headers for data structure references (mach_vmstat, mach_swap, etc.)

### Code Review Focus

- **Indentation**: Must use TABS (not spaces)
- **Error handling**: Fail-fast pattern (immediate return on error)
- **Extern declarations**: Proper usage in fetch functions
- **File permissions**: 644 for all new files
- **Copyright headers**: Follow existing pattern (Red Hat, GPL-2.0+)

---

## Rollback Plan

If a step fails:

1. **Git revert** the problematic commit
2. **Analyze test failures** to understand root cause
3. **Fix issues** in isolated branch
4. **Re-test** before merging
5. **Continue** with next step only after current step passes

---

## Implementation Workflow (Per Step)

### For metrictab extraction:
1. **Create metrics.h** with extern declarations
2. **Create metrics.c** with moved metrictab array
3. **Update pmda.c**:
   - Add `#include "metrics.h"`
   - Remove metrictab[] definition
   - Remove metrictab_sz calculation
4. **Update GNUmakefile** (add metrics.c and metrics.h)
5. **Commit changes** to git
6. **Run tests** via macos-darwin-pmda-qa agent
7. **Run code review** via pcp-code-reviewer agent
8. **Fix any issues** identified
9. **Re-test** if code was modified
10. **Proceed** to function extractions

### For each function extraction:
1. **Create header file** (.h) with type definitions and function declarations
2. **Create source file** (.c) with refresh and fetch implementations
3. **Update pmda.c**:
   - Add include
   - Remove embedded fetch function
   - Update dispatcher case
4. **Update kernel.c** (if refresh function needs moving)
5. **Update GNUmakefile** (add to CFILES and HFILES)
6. **Commit changes** to git
7. **Run tests** via macos-darwin-pmda-qa agent
8. **Run code review** via pcp-code-reviewer agent
9. **Fix any issues** identified
10. **Re-test** if code was modified
11. **Proceed** to next step

---

## Final Deliverables

1. **New files**: metrics.{c,h}, vmstat.{c,h}, filesys.{c,h}, cpu.{c,h}, nfs.{c,h}
2. **Refactored pmda.c**: ~960 lines shorter (~500 lines remaining), cleaner dispatch logic
3. **Updated GNUmakefile**: Includes all new files
4. **Test results**: All tests passing (unit + integration)
5. **Documentation**: Updated comments explaining modular architecture

---

## Estimated Impact

- **Code removed from pmda.c**: ~960 lines (784 metrictab + ~179 fetch functions)
- **Code added to new files**: ~1400 lines (with headers, includes, comments)
- **Net increase**: ~440 lines (due to proper headers and documentation)
- **pmda.c final size**: ~500 lines (down from 1456)
- **Maintainability gain**: VERY HIGH - clean separation of concerns
- **Test coverage**: Maintained at 100%
- **Functional changes**: ZERO
