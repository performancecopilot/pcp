# Darwin PMDA Enhancement Plan

## Current Status

**Last Updated:** 2026-01-02
**Current Step:** Step 2.3 completed, Step 2.4 (next to investigate)

## Progress Tracker

| Step | Status | Notes |
|------|--------|-------|
| 1.1a | COMPLETED | vm_statistics64 API upgrade (commit b49d238c85) |
| 1.1b | COMPLETED | Memory compression metrics (commit 9db0865001) |
| 1.2 | COMPLETED | VFS statistics (commit 71f12b992a) |
| 2.1 | COMPLETED | UDP protocol statistics (commit daf0f8c892) |
| 2.2 | COMPLETED | ICMP protocol statistics (commit 14654a6e2a) |
| 2.3 | COMPLETED | Socket counts (commit 50ab438ac3) |
| 2.4 | NEXT | TCP connection states (investigation required) |
| 2.5 | PENDING | TCP limitation documentation |
| 3.1 | PENDING | Process I/O statistics |
| 3.2 | PENDING | Enhanced process metrics |
| 4.1 | PENDING | Transform plan → permanent documentation |

---

## Execution Workflow

1. **Execute one step at a time** - complete implementation, tests, docs
2. **Run test runner** after each step (or smaller units for feedback)
3. **Run code review** - use `pcp-code-reviewer` agent to validate code quality and PCP standards
4. **Fix any issues** - address critical and important findings from code review
5. **Pause for user review** - present changes for review
6. **Wait for user approval** - user signals when to commit
7. **Commit only after approval** - then proceed to next step

---

## Critical Files

| File | Purpose |
|------|---------|
| `src/pmdas/darwin/pmda.c` | Main PMDA: metrictab[], clusters, fetch callbacks |
| `src/pmdas/darwin/kernel.c` | Data collection: refresh_*() functions |
| `src/pmdas/darwin/darwin.h` | Structure definitions |
| `src/pmdas/darwin/help` | Metric documentation |
| `scripts/darwin/test/unit/test-*.txt` | Unit tests (dbpmda) |
| `scripts/darwin/test/integration/run-integration-tests.sh` | Integration tests |

---

## Phase 1: Memory Enhancement

### Step 1.1a: COMPLETED - vm_statistics64 API Upgrade

Commit: `b49d238c85`

Changes made:
- `kernel.c`: Changed `refresh_vmstat()` to use `host_statistics64(HOST_VM_INFO64)`
- `pmda.c`: Changed `struct vm_statistics` to `struct vm_statistics64`
- Added `test-vmstat64-upgrade.txt` unit test

BREAKING CHANGE: Memory counters now 64-bit (affects archive compatibility)

---

### Step 1.1b: NEXT - Memory Compression Metrics

**Goal:** Add metrics exclusive to `vm_statistics64`

**New Metrics:**

| Metric | Item | Type | Semantics | Source |
|--------|------|------|-----------|--------|
| `mem.util.compressed` | 130 | U64 | instant (KB) | compressor_page_count × pagesize |
| `mem.compressions` | 131 | U64 | counter | vm_statistics64.compressions |
| `mem.decompressions` | 132 | U64 | counter | vm_statistics64.decompressions |
| `mem.compressor.pages` | 133 | U32 | instant | vm_statistics64.compressor_page_count |
| `mem.compressor.uncompressed_pages` | 134 | U64 | instant | vm_statistics64.total_uncompressed_pages_in_compressor |

**Changes Required:**

1. **pmda.c** - Add 5 entries to metrictab[] (CLUSTER_VMSTAT, items 130-134)
2. **pmda.c** - Add case 130 to `fetch_vmstat()` for computed `mem.util.compressed`
3. **pmns** - Add new metric names to namespace hierarchy
4. **help** - Add documentation for 5 new metrics

**Implementation Pattern:**

**metrictab[] entries** (in `pmda.c`):
```c
// Direct pointer for fields that map directly:
/* mem.compressions - item 131 */
  { &mach_vmstat.compressions,
    { PMDA_PMID(CLUSTER_VMSTAT,131), PM_TYPE_U64, PM_INDOM_NULL,
      PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
```

**PMNS entries** (in `src/pmdas/darwin/pmns`):
```
mem {
    ...
    compressions	DARWIN:1:131
    decompressions	DARWIN:1:132
    compressor
}

mem.compressor {
    pages		DARWIN:1:133
    uncompressed_pages	DARWIN:1:134
}
```

**fetch_*() for computed values** (in `pmda.c`):
```c
// In fetch_vmstat() - computed value for mem.util.compressed:
    case 130: /* mem.util.compressed */
        atom->ull = page_count_to_kb(mach_vmstat.compressor_page_count);
        return 1;
```

**Tests:**
- Create `scripts/darwin/test/unit/test-memory-compression.txt` (see "Unit Test Pattern")
- Add integration test validation to `run-integration-tests.sh` (see "Integration Test Pattern")
- Invoke `macos-darwin-pmda-qa` agent to validate changes

---

### Step 1.2: VFS Statistics

**Goal:** Add system resource metrics via sysctl

**New Cluster:** `CLUSTER_VFS` (12)

**New Metrics:**

| Metric | Item | Type | Semantics | Source (sysctl) |
|--------|------|------|-----------|-----------------|
| `vfs.files.count` | 135 | U32 | instant | kern.num_files |
| `vfs.files.max` | 136 | U32 | discrete | kern.maxfiles |
| `vfs.files.free` | 137 | U32 | instant | computed: max - count |
| `vfs.vnodes.count` | 138 | U32 | instant | kern.num_vnodes |
| `vfs.vnodes.max` | 139 | U32 | discrete | kern.maxvnodes |
| `kernel.all.nprocs` | 140 | U32 | instant | kern.num_tasks |
| `kernel.all.nthreads` | 141 | U32 | instant | kern.num_threads |

**Changes Required:**

1. Add `CLUSTER_VFS` to cluster enum in pmda.c
2. Add global: `struct vfsstats mach_vfs`, `int mach_vfs_error`
3. Add `struct vfsstats` to darwin.h
4. Add `refresh_vfs()` to kernel.c using sysctlbyname()
5. Add 7 entries to metrictab[]
6. **Update PMNS file** with VFS metric hierarchy
7. **Update root file** to include `vfs` in root{} block
8. Add `fetch_vfs()` for computed `vfs.files.free`
9. Add dispatch cases to darwin_refresh() and darwin_fetchCallBack()
10. Add help text
11. Create unit test: `test-vfs.txt` (follow "Unit Test Pattern")
12. Add integration test validation (follow "Integration Test Pattern")
13. Invoke `macos-darwin-pmda-qa` agent to validate changes

**Status:** COMPLETED

**Implementation Notes:**
- All 7 metrics successfully implemented following modular pattern
- **Code organization**: Created dedicated `vfs.c` and `vfs.h` files (following disk.c/network.c pattern)
  - `vfs.h`: vfsstats_t structure and function declarations
  - `vfs.c`: refresh_vfs() and fetch_vfs() implementations
  - `pmda.c`: cluster definition, metrictab entries, dispatch wiring
  - `GNUmakefile`: Updated to compile vfs.c
- PMNS namespace created with `vfs.files.*` and `vfs.vnodes.*` hierarchies
- **Important lesson learned #1**: When adding a new top-level namespace, the `src/pmdas/darwin/root` file MUST be updated to include it in the `root{}` block. Initial implementation forgot this step, causing "Disconnected subtree" PMNS parsing errors during build.
- **Important lesson learned #2**: New source files must have proper permissions (644, not 600) for build isolation to work
- Unit tests created in `test-vfs.txt` covering all 7 metrics
- Integration tests added to validate all metrics work correctly
- All tests passing via `macos-darwin-pmda-qa` agent

---

## Phase 2: Network Enhancement

### Step 2.1: UDP Protocol Statistics

**New Cluster:** `CLUSTER_UDP` (13)

**API:** `sysctlbyname("net.inet.udp.stats", &udpstat, &size, NULL, 0)`

| Metric | Item | Type | Source (struct udpstat) |
|--------|------|------|-------------------------|
| `network.udp.indatagrams` | 142 | U64 | udps_ipackets |
| `network.udp.outdatagrams` | 143 | U64 | udps_opackets |
| `network.udp.noports` | 144 | U64 | udps_noport |
| `network.udp.inerrors` | 145 | U64 | computed sum |
| `network.udp.rcvbuferrors` | 146 | U64 | udps_fullsock |

**Status:** COMPLETED

**Implementation Notes:**
- All 5 UDP metrics successfully implemented following modular pattern
- **Code organization**: Created dedicated `udp.c` and `udp.h` files (following vfs/disk/network pattern)
  - `udp.h`: udpstats_t structure and function declarations
  - `udp.c`: refresh_udp() and fetch_udp() implementations
  - `pmda.c`: CLUSTER_UDP definition, metrictab entries, dispatch wiring
  - `GNUmakefile`: Updated to compile udp.c
- PMNS namespace created with `network.udp.*` hierarchy under existing network namespace
- Data conversion: kernel udpstat fields are u_int32_t, converted to u_int64_t for PMDA export
- Computed metric: `network.udp.inerrors` aggregates hdrops + badsum + badlen
- Unit tests created in `test-udp.txt` covering all 5 metrics
- Integration tests added to validate all metrics work correctly
- All tests passing via `macos-darwin-pmda-qa` agent

---

### Step 2.2: ICMP Protocol Statistics

**New Cluster:** `CLUSTER_ICMP` (14)

**API:** `sysctlbyname("net.inet.icmp.stats", &icmpstat, &size, NULL, 0)`

| Metric | Item | Type | Source |
|--------|------|------|--------|
| `network.icmp.inmsgs` | 147 | U64 | sum of icps_inhist[] |
| `network.icmp.outmsgs` | 148 | U64 | sum of icps_outhist[] |
| `network.icmp.inerrors` | 149 | U64 | computed sum |
| `network.icmp.indestunreachs` | 150 | U64 | icps_inhist[ICMP_UNREACH] |
| `network.icmp.inechos` | 151 | U64 | icps_inhist[ICMP_ECHO] |
| `network.icmp.inechoreps` | 152 | U64 | icps_inhist[ICMP_ECHOREPLY] |
| `network.icmp.outechos` | 153 | U64 | icps_outhist[ICMP_ECHO] |
| `network.icmp.outechoreps` | 154 | U64 | icps_outhist[ICMP_ECHOREPLY] |

**Status:** COMPLETED

**Implementation Notes:**
- All 8 ICMP metrics successfully implemented following modular pattern
- **Code organization**: Created dedicated `icmp.c` and `icmp.h` files (following vfs/disk/udp pattern)
  - `icmp.h`: icmpstats_t structure and function declarations
  - `icmp.c`: refresh_icmp() and fetch_icmp() implementations
  - `pmda.c`: CLUSTER_ICMP definition, metrictab entries, dispatch wiring
  - `GNUmakefile`: Updated to compile icmp.c
- PMNS namespace created with `network.icmp.*` hierarchy under existing network namespace
- Data aggregation: inmsgs/outmsgs computed from icps_inhist[]/icps_outhist[] arrays (41 elements each)
- Computed metrics:
  - `inmsgs` = sum of all icps_inhist[] entries
  - `outmsgs` = sum of all icps_outhist[] entries
  - `inerrors` = icps_error + icps_badcode + icps_tooshort + icps_checksum + icps_badlen
- Specific message types extracted from histogram arrays using ICMP type constants (ICMP_ECHO=8, ICMP_ECHOREPLY=0, ICMP_UNREACH=3)
- Unit tests created in `test-icmp.txt` covering all 8 metrics
- Integration tests added to validate all metrics work correctly
- All tests passing via `macos-darwin-pmda-qa` agent

---

### Step 2.3: Socket Counts

**New Cluster:** `CLUSTER_SOCKSTAT` (15)

**API:** `sysctlbyname("net.inet.{tcp,udp}.pcbcount", ...)`

| Metric | Item | Type | Source (sysctl) |
|--------|------|------|-----------------|
| `network.sockstat.tcp.inuse` | 155 | U32 | net.inet.tcp.pcbcount |
| `network.sockstat.udp.inuse` | 156 | U32 | net.inet.udp.pcbcount |

**Status:** COMPLETED

**Implementation Notes:**
- Both socket statistics successfully implemented following modular pattern
- **Code organization**: Created dedicated `sockstat.c` and `sockstat.h` files (following vfs/udp/icmp pattern)
  - `sockstat.h`: sockstats_t structure and function declarations
  - `sockstat.c`: refresh_sockstat() implementation
  - `pmda.c`: CLUSTER_SOCKSTAT definition, metrictab entries, dispatch wiring
  - `GNUmakefile`: Updated to compile sockstat.c
- PMNS namespace created with `network.sockstat.tcp.inuse` and `network.sockstat.udp.inuse`
- Simpler implementation than UDP/ICMP: no computed values, direct metrictab pointers only
- Metrics use sysctlbyname() to fetch PCB (Protocol Control Block) counts
- PCB count represents active sockets/connections for each protocol
- Unit tests created in `test-sockstat.txt` covering both metrics
- Integration tests added to validate all metrics work correctly
- All tests passing via `macos-darwin-pmda-qa` agent

---

### Step 2.4: TCP Connection States (Investigation Required)

**API:** `sysctl("net.inet.tcp.pcblist")` returns binary xinpgen structures

Potential metrics: `network.tcp.established`, `network.tcp.time_wait`, `network.tcp.listen`, etc.

**Note:** May be complex - requires parsing pcblist structures.

---

### Step 2.5: TCP Stats Limitation Documentation

Document that detailed TCP statistics (retransmits, timeouts, segment counts) are blocked by `net.inet.tcp.disable_access_to_stats=1` which is SIP-protected.

---

## Phase 3: Process Enhancement

### Step 3.1: Process I/O Statistics

**Files:** `src/pmdas/darwin_proc/pmda.c`, `kinfo_proc.c`, `kinfo_proc.h`

**API:** `proc_pid_rusage(pid, RUSAGE_INFO_V3, &rusage)`

| Metric | Type | Source |
|--------|------|--------|
| `proc.io.read_bytes` | U64 | rusage_info_v3.ri_diskio_bytesread |
| `proc.io.write_bytes` | U64 | rusage_info_v3.ri_diskio_byteswritten |

---

### Step 3.2: Enhanced Process Metrics

| Metric | Type | Source |
|--------|------|--------|
| `proc.memory.vmsize` | U64 | pti_virtual_size |
| `proc.fd.count` | U32 | proc_pidinfo(PROC_PIDLISTFDS) |

---

## Phase 4: Finalization

### Step 4.1: Transform Plan Document

**Goal:** Convert this working plan into permanent documentation

**Tasks:**
1. Update document title to `DARWIN-PMDA-DEVELOPMENT.md` (rename file)
2. Remove "Current Status" and "Progress Tracker" sections (no longer needed)
3. Remove "Execution Workflow" section (implementation complete)
4. Keep all valuable permanent documentation:
   - Implementation patterns (Code Organization, metrictab patterns, etc.)
   - Test patterns (Unit Test Pattern, Integration Test Pattern)
   - Technical notes (sysctl info, API limitations, etc.)
   - Completed work summary (for historical reference)
5. Add "Completed Enhancements" summary section at top listing all implemented features
6. Update any "NEXT" or "PENDING" status markers to "COMPLETED"
7. Consider whether any lessons learned should be added to help future contributors

**Result:** The document becomes a guide for future darwin PMDA development, explaining established patterns and architectural decisions made during this enhancement effort.

---

## Key Technical Notes

1. **sysctl() is a syscall** - not fork/exec, safe and efficient
2. **TCP detailed stats blocked** - SIP-protected kernel setting
3. **UDP/ICMP stats work** - via sysctl struct access
4. **Each step independently committable** - with full test coverage
5. **No Claude Code commit footer** - user preference

---

## Error Handling and Code Quality

### Error Handling Pattern: Fail Fast

The darwin PMDA uses a **fail-fast** error handling pattern throughout. This is the standard for all refresh functions:

**✅ CORRECT Pattern (Fail Fast):**
```c
int refresh_example(example_t *data)
{
    size_t size = sizeof(data->field);

    if (sysctlbyname("kern.example", &data->field, &size, NULL, 0) == -1)
        return -oserror();  // Return immediately on error

    // Only continues if sysctl succeeded
    return 0;
}
```

**❌ INCORRECT Pattern (Error Accumulation):**
```c
int refresh_example(example_t *data)
{
    size_t size;
    int error = 0;  // DON'T DO THIS

    size = sizeof(data->field1);
    if (sysctlbyname("kern.field1", &data->field1, &size, NULL, 0) == -1)
        error = -oserror();  // Sets error

    size = sizeof(data->field2);
    if (sysctlbyname("kern.field2", &data->field2, &size, NULL, 0) == -1)
        error = -oserror();  // OVERWRITES previous error!

    return error;  // Only returns LAST error
}
```

**Why Fail Fast:**
- Matches existing darwin PMDA conventions (refresh_vmstat, refresh_swap, etc.)
- Prevents error information loss (early errors aren't overwritten)
- On macOS, if one sysctl fails, it's often systemic (permissions, kernel version)
- No resource cleanup needed (sysctls are direct kernel calls)
- Cleaner, more readable code

**Exception:** Only use try-all pattern when you have resources to cleanup (file handles, allocated memory). See Linux PMDA's `refresh_proc_sys_fs()` for this pattern.

### Code Review Process

**Use the `pcp-code-reviewer` agent** after implementing each step to validate:
- Code quality and style consistency
- Error handling patterns
- PCP coding conventions
- Documentation completeness
- Integration with existing code

**Example workflow:**
1. Implement feature
2. Run tests (`macos-darwin-pmda-qa` agent)
3. **Run code review** (`pcp-code-reviewer` agent)
4. Fix critical and important issues identified
5. Re-test if code was modified
6. Commit after approval

**Lessons Learned:**
- VFS implementation initially used error accumulation (fixed in commit d431bae047)
- Code reviewer correctly identified the anti-pattern
- Always validate error handling matches darwin PMDA conventions

---

## Test Patterns and Test Running

### Unit Test Pattern

Unit tests are dbpmda command files located in `scripts/darwin/test/unit/test-*.txt`.

**Format**: Simple text files with `desc` and `fetch` commands

**Example** (`test-memory-compression.txt`):
```
# Test memory compression metrics (vm_statistics64 exclusive)
# These metrics are only available via the vm_statistics64 API
# and provide insight into macOS memory compression activity.

# Compressed memory utilization
desc mem.util.compressed
fetch mem.util.compressed

# Compression activity counters
desc mem.compressions
fetch mem.compressions

desc mem.decompressions
fetch mem.decompressions

# Compressor state metrics
desc mem.compressor.pages
fetch mem.compressor.pages

desc mem.compressor.uncompressed_pages
fetch mem.compressor.uncompressed_pages
```

**To add unit tests**:
1. Create `scripts/darwin/test/unit/test-<feature>.txt`
2. Add `desc <metric>` and `fetch <metric>` for each new metric
3. Include comments explaining what the metrics test
4. Follow the pattern in existing test files like `test-basic.txt` and `test-memory-compression.txt`

### Integration Test Pattern

Integration tests validate metrics through real PCP tools (pminfo, pmval, pmstat).

**Location**: `scripts/darwin/test/integration/run-integration-tests.sh`

**Pattern**: Add test cases using the `run_test()` and `validate_metric()` helper functions

**Example additions**:
```bash
# Test Group: VFS Metrics
echo "Test Group: VFS Metrics"
run_test "vfs.files.count exists" "pminfo -f vfs.files.count"
run_test "vfs.files.max > 0" "validate_metric vfs.files.max positive"
run_test "vfs.vnodes.count >= 0" "validate_metric vfs.vnodes.count non-negative"
echo
```

**Validation options**:
- `"exists"` - metric exists and is fetchable
- `"positive"` - metric value > 0
- `"non-negative"` - metric value >= 0

**To add integration tests**:
1. Open `scripts/darwin/test/integration/run-integration-tests.sh`
2. Add a new test group section before the Summary
3. Use `run_test()` for each metric validation
4. Use appropriate validation (exists/positive/non-negative)
5. Follow the pattern of existing test groups in the file

### Test Running Workflow

**CRITICAL**: The `macos-darwin-pmda-qa` agent uses `git archive` via Makepkgs to build the PMDA. **Any uncommitted changes will NOT be included in the build!** Always commit your changes to git before invoking the test agent.

**Workflow**:

1. **Commit all changes to git first** - The agent only sees committed code
2. **After committing**, invoke the `macos-darwin-pmda-qa` agent
3. The agent will:
   - Build the PMDA in an isolated environment
   - Run unit tests using dbpmda
   - Run integration tests if PCP is installed
   - Report results back to you
3. **Analyze the results** from the agent's test output
4. If tests fail, use the failure information to debug and fix issues
5. Re-invoke the agent after making fixes

**Only create additional diagnostic code/tests** if the existing test output doesn't provide enough information to understand what's broken. Ask the user for input before adding extra debugging code if you're unsure.

---

## Code Organization Pattern

**When to create separate .c/.h files for a new subsystem:**

The Darwin PMDA follows a **modular, single-responsibility** pattern. Create dedicated files when:

- Adding a **new cluster** (CLUSTER_VFS, CLUSTER_UDP, etc.)
- The subsystem has its **own data structures** and **refresh logic**
- Following the pattern: `disk.c/disk.h`, `network.c/network.h`, `vfs.c/vfs.h`

**File structure for new subsystems:**

1. **Create `<subsystem>.h`:**
   - Define the data structure: `typedef struct <subsystem>stats { ... } <subsystem>stats_t;`
   - Declare function prototypes: `extern int refresh_<subsystem>(<subsystem>stats_t *);`
   - Declare fetch function: `extern int fetch_<subsystem>(unsigned int, pmAtomValue *);`
   - Use proper copyright header (see disk.h/network.h/vfs.h as examples)
   - Ensure file permissions are **644** (rw-r--r--)

2. **Create `<subsystem>.c`:**
   - Include necessary headers: `pmapi.h`, `pmda.h`, `<subsystem>.h`
   - Implement `refresh_<subsystem>()` - data collection logic
   - Implement `fetch_<subsystem>()` - computed values only (direct values go in metrictab)
   - Use `extern` declarations for globals from pmda.c (e.g., `extern <subsystem>stats_t mach_<subsystem>;`)
   - Ensure file permissions are **644** (rw-r--r--)

3. **Update `GNUmakefile`:**
   - Add to `CFILES`: `<subsystem>.c`
   - Add to `HFILES`: `<subsystem>.h`

4. **Update `pmda.c`:**
   - Add `#include "<subsystem>.h"` at top
   - Declare globals: `int mach_<subsystem>_error = 0;` and `<subsystem>stats_t mach_<subsystem> = { 0 };`
   - Add metrictab entries (direct value pointers)
   - Add dispatch to `darwin_refresh()` and `darwin_fetchCallBack()`
   - **Do NOT** put `refresh_*()` or large fetch logic in pmda.c - keep it modular

**Benefits of this pattern:**
- Keeps pmda.c focused on coordination, not implementation
- Each subsystem is self-contained and testable
- Matches PCP project patterns (e.g., Linux PMDA's proc_sys_fs.c)
- Easier maintenance and code review

---

## Implementation Pattern Summary

For each new metric:

1. Define cluster enum (if new cluster)
2. **Consider creating separate .c/.h files** (see "Code Organization Pattern" above)
3. Declare global data structure and error flag in pmda.c
4. Implement `refresh_*()` function in dedicated subsystem.c file (or kernel.c for simple cases)
5. Add metrics to metrictab[] in pmda.c
6. **Update PMNS file** (`src/pmdas/darwin/pmns`)
   - Add metric names in hierarchical structure
   - Format: `metricname DARWIN:cluster:item`
   - Add any new metric hierarchies (e.g., `mem.compressor`)
   - **CRITICAL**: Forgetting this step causes metric lookup failures
   - **CRITICAL**: If adding a new top-level namespace (e.g., `vfs`), MUST also update `src/pmdas/darwin/root` to include it in the root{} block, otherwise you get "Disconnected subtree" PMNS errors
7. **Update root file** (if new top-level namespace) - add to root{} block in `src/pmdas/darwin/root`
8. Add `fetch_*()` function in subsystem.c (for computed values only)
9. Add case to `darwin_fetchCallBack()` dispatch in pmda.c
10. Add to `darwin_refresh()` in pmda.c
11. Add help text in `src/pmdas/darwin/help`
12. Add unit test (test-*.txt) - see "Unit Test Pattern" below
13. Add integration test validation - see "Integration Test Pattern" below
14. **Invoke macos-darwin-pmda-qa agent** to run tests and validate changes
