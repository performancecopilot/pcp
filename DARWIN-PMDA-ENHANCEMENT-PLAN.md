# Darwin PMDA Enhancement Plan

## Current Status

**Last Updated:** 2026-01-03
**Current Step:** Step 2.5-pre completed, Step 2.5a next (TCP basic implementation)
**Pull Request:** https://github.com/performancecopilot/pcp/pull/2442

## Progress Tracker

| Step | Status | Notes |
|------|--------|-------|
| 1.1a | COMPLETED | vm_statistics64 API upgrade (commit b49d238c85) |
| 1.1b | COMPLETED | Memory compression metrics (commit 9db0865001) |
| 1.2 | COMPLETED | VFS statistics (commit 71f12b992a) |
| 2.1 | COMPLETED | UDP protocol statistics (commit daf0f8c892) |
| 2.2 | COMPLETED | ICMP protocol statistics (commit 14654a6e2a) |
| 2.3 | COMPLETED | Socket counts (commit 50ab438ac3) |
| 2.4 | COMPLETED | TCP connection states (commit 96a4191fcd) |
| 2.5-pre | COMPLETED | Enable TCP stats in Cirrus CI |
| 2.5a | NEXT | TCP protocol statistics - basic implementation (15 metrics, Linux parity) |
| 2.5b | PENDING | TCP statistics - detection and documentation (warnings + man page) |
| 2.5c | DEFERRED | TCP statistics - auto-enable config (for maintainer discussion) |
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

### Step 2.4: TCP Connection States

**Goal:** Add metrics for TCP connection state counts

**Status:** COMPLETED

**Commit:** `96a4191fcd`

**Implementation Notes:**
- All 11 TCP connection state metrics successfully implemented
- **Code organization**: Created dedicated `tcpconn.c` and `tcpconn.h` files
  - `tcpconn.h`: tcpconn_stats_t structure and function declaration
  - `tcpconn.c`: refresh_tcpconn() implementation using binary PCB list parsing
  - `pmda.c`: CLUSTER_TCPCONN (16) definition, metrictab entries, dispatch wiring
  - `GNUmakefile`: Updated to compile tcpconn.c
- **API**: Uses `sysctlbyname("net.inet.tcp.pcblist64")` for 64-bit PCB structures
- **Parsing**: Binary structure iteration using xinpgen header and xtcpcb64 entries
- **Performance**: 10MB hardcoded soft limit with debug-level logging for large buffers
- **Cross-platform**: Metric namespace `network.tcpconn.*` matches Linux PMDA exactly

**New Metrics** (all U32, INSTANT, PM_INDOM_NULL):

| Metric | Item | TCP State | Description |
|--------|------|-----------|-------------|
| `network.tcpconn.established` | 157 | TCPS_ESTABLISHED | Active established connections |
| `network.tcpconn.syn_sent` | 158 | TCPS_SYN_SENT | Active opening (SYN sent) |
| `network.tcpconn.syn_recv` | 159 | TCPS_SYN_RECEIVED | Passive opening (SYN received) |
| `network.tcpconn.fin_wait1` | 160 | TCPS_FIN_WAIT_1 | Active close, FIN sent |
| `network.tcpconn.fin_wait2` | 161 | TCPS_FIN_WAIT_2 | Active close, FIN acked |
| `network.tcpconn.time_wait` | 162 | TCPS_TIME_WAIT | 2MSL wait state |
| `network.tcpconn.close` | 163 | TCPS_CLOSED | Closed connections |
| `network.tcpconn.close_wait` | 164 | TCPS_CLOSE_WAIT | Passive close, waiting |
| `network.tcpconn.last_ack` | 165 | TCPS_LAST_ACK | Passive close, FIN sent |
| `network.tcpconn.listen` | 166 | TCPS_LISTEN | Listening for connections |
| `network.tcpconn.closing` | 167 | TCPS_CLOSING | Simultaneous close |

**Testing:**
- Unit tests: `test-tcpconn.txt` covering all 11 metrics
- Integration tests: 15 test cases validating existence and non-negative values
- All tests passing via `macos-darwin-pmda-qa` agent

**Code Review:**
- pcp-code-reviewer verdict: **APPROVED - READY TO MERGE**
- Excellent ratings across all categories (code quality, error handling, testing, documentation)
- Follows fail-fast error handling pattern
- Proper memory management (malloc/free)
- Consistent with existing darwin PMDA patterns (sockstat, udp, icmp)

---

### Step 2.5: TCP Protocol Statistics (Multi-Part)

**Goal:** Add TCP protocol statistics with Linux PMDA parity, proper access control handling, and documentation

**Status:** PLANNING - Research complete, breaking into 3 implementable steps

**Overall API:** `sysctlbyname("net.inet.tcp.stats", &tcpstat, &size, NULL, 0)`

---

#### Step 2.5-pre: Enable TCP Stats in Cirrus CI

**Goal:** Configure test environment to enable TCP statistics before PCP installation

**Status:** COMPLETED

**Why First:** Validates we can control the test VM environment and ensures tests will pass for subsequent steps

**Changes Made:**

Added new script step in `.cirrus.yml` before `pcp_build_script`:

```yaml
  enable_tcp_stats_script: |
    echo "Enabling TCP statistics for testing..."
    sudo sysctl -w net.inet.tcp.disable_access_to_stats=0
    echo "Verifying setting:"
    sysctl net.inet.tcp.disable_access_to_stats
```

**Rationale:** Must be done before PCP build/install so PMCD starts with stats already enabled

**Implementation Notes:**
- Added as a separate script task in Cirrus CI pipeline
- Runs after Homebrew cache setup but before PCP build
- Uses `sudo sysctl -w` to set the kernel parameter
- Verifies the setting was applied with a read-back check
- This ensures TCP statistics are available when PMCD starts after installation

**Testing:** Verify with `cirrus run` that the setting takes effect in CI environment

---

#### Step 2.5a: Basic TCP Statistics Implementation

**Goal:** Implement core TCP statistics matching Linux PMDA parity

**Status:** NOT STARTED

**Assumption:** User has already set `net.inet.tcp.disable_access_to_stats=0` (or Cirrus CI has done it)

**New Cluster:** `CLUSTER_TCP` (17)

**Metrics to Implement** (15 core metrics for Linux parity):

| Metric | Item | Type | Darwin Source (struct tcpstat) | Linux Equivalent |
|--------|------|------|-------------------------------|------------------|
| `network.tcp.activeopens` | 168 | U64 | tcps_connattempt | activeopens |
| `network.tcp.passiveopens` | 169 | U64 | tcps_accepts | passiveopens |
| `network.tcp.attemptfails` | 170 | U64 | tcps_conndrops | attemptfails |
| `network.tcp.estabresets` | 171 | U64 | tcps_drops | estabresets |
| `network.tcp.currestab` | 172 | U32 | computed from tcpconn state | currestab |
| `network.tcp.insegs` | 173 | U64 | tcps_rcvtotal | insegs |
| `network.tcp.outsegs` | 174 | U64 | tcps_sndtotal | outsegs |
| `network.tcp.retranssegs` | 175 | U64 | tcps_sndrexmitpack | retranssegs |
| `network.tcp.inerrs` | 176 | U64 | computed (see below) | inerrs |
| `network.tcp.outrsts` | 177 | U64 | tcps_sndctrl (partial) | outrsts |
| `network.tcp.incsumerrors` | 178 | U64 | tcps_rcvbadsum | incsumerrors |
| `network.tcp.rtoalgorithm` | 179 | U32 | constant (4) | rtoalgorithm |
| `network.tcp.rtomin` | 180 | U32 | constant (200) | rtomin |
| `network.tcp.rtomax` | 181 | U32 | constant (64000) | rtomax |
| `network.tcp.maxconn` | 182 | U32 | constant (-1) | maxconn |

**Notes on Mappings:**
- `currestab`: Use existing `network.tcpconn.established` value (already implemented in Step 2.4)
- `inerrs`: Computed as `tcps_rcvbadsum + tcps_rcvbadoff + tcps_rcvshort + tcps_rcvmemdrop`
- Algorithm/timing constants: Match Linux values (Van Jacobson's algorithm=4, 200ms min, 64s max)
- `maxconn`: -1 indicates no fixed limit (same as Linux)

**Changes Required:**

1. **tcp.h** - Create header file:
   ```c
   typedef struct tcpstats {
       struct tcpstat stats;  /* from netinet/tcp_var.h */
   } tcpstats_t;

   extern int refresh_tcp(tcpstats_t *);
   ```

2. **tcp.c** - Implement refresh function:
   ```c
   int refresh_tcp(tcpstats_t *tcp)
   {
       size_t size = sizeof(tcp->stats);
       if (sysctlbyname("net.inet.tcp.stats", &tcp->stats, &size, NULL, 0) == -1)
           return -oserror();
       return 0;
   }
   ```

3. **pmda.c**:
   - Add `#include "tcp.h"`
   - Add `CLUSTER_TCP` to cluster enum (value 17)
   - Add globals: `int mach_tcp_error = 0; tcpstats_t mach_tcp = { 0 };`
   - Add 15 metrictab entries (direct pointers for most, `fetch_tcp()` for computed)
   - Add dispatch in `darwin_refresh()`: `if (need_refresh[CLUSTER_TCP]) mach_tcp_error = refresh_tcp(&mach_tcp);`
   - Add `fetch_tcp()` function for computed metrics (inerrs, currestab)
   - Add case in `darwin_fetchCallBack()` for CLUSTER_TCP

4. **pmns** - Add `network.tcp.*` hierarchy (15 metrics)

5. **help** - Add basic help text for 15 metrics (NO access control warnings yet)

6. **GNUmakefile** - Add tcp.c and tcp.h to build

7. **Unit tests** - Create `test-tcp.txt`:
   ```
   # Test TCP protocol statistics
   desc network.tcp.activeopens
   fetch network.tcp.activeopens
   desc network.tcp.insegs
   fetch network.tcp.insegs
   # ... all 15 metrics
   ```

8. **Integration tests** - Add to `run-integration-tests.sh`:
   ```bash
   echo "Test Group: TCP Statistics"
   run_test "tcp.activeopens exists" "pminfo -f network.tcp.activeopens"
   run_test "tcp.insegs non-negative" "validate_metric network.tcp.insegs non-negative"
   # ... test all 15 metrics
   ```

**What's NOT in this step:**
- NO warning messages about access control
- NO configuration file
- NO man page
- NO auto-enable feature

**Testing:** With Cirrus CI having enabled the flag, all tests should pass

---

#### Step 2.5b: Access Control Detection and Documentation

**Goal:** Add detection of disabled stats and comprehensive user documentation

**Status:** NOT STARTED

**Depends On:** Step 2.5a complete

**Changes Required:**

1. **pmda.c** - Add startup check in `darwin_init()`:

```c
static void
check_tcp_stats_access(void)
{
    int flag_value = 1;
    size_t len = sizeof(flag_value);

    if (sysctlbyname("net.inet.tcp.disable_access_to_stats",
                     &flag_value, &len, NULL, 0) == 0) {
        if (flag_value != 0) {
            pmNotifyErr(LOG_WARNING,
                "TCP statistics access is DISABLED (net.inet.tcp.disable_access_to_stats=%d).\n"
                "All network.tcp.* metrics will report zero values.\n"
                "\n"
                "To enable TCP statistics, run as root:\n"
                "    sudo sysctl -w net.inet.tcp.disable_access_to_stats=0\n"
                "\n"
                "To make this permanent across reboots, add to /etc/sysctl.conf:\n"
                "    net.inet.tcp.disable_access_to_stats=0\n"
                "\n"
                "See pmdadarwin(1) for more information.",
                flag_value);
        }
    }
}
```

Call from `darwin_init()` after `pmdaInit()`.

2. **pmdadarwin.1** - Create first man page for darwin PMDA:

```nroff
.TH PMDADARWIN 1 "PCP" "Performance Co-Pilot"
.SH NAME
pmdadarwin \- Darwin (macOS) kernel PMDA
.SH SYNOPSIS
\f3$PCP_PMDAS_DIR/darwin/pmdadarwin\f1
[\f3\-d\f1 \f2domain\f1]
[\f3\-l\f1 \f2logfile\f1]
[\f3\-U\f1 \f2username\f1]
.SH DESCRIPTION
.B pmdadarwin
is a Performance Metrics Domain Agent (PMDA) which extracts
performance metrics from the macOS (Darwin) kernel...

.SH TCP STATISTICS CONFIGURATION
On macOS, access to detailed TCP protocol statistics is controlled
by the kernel sysctl parameter
.BR net.inet.tcp.disable_access_to_stats .
By default, this is set to 1 (disabled) for privacy and security.

When disabled, all
.B network.tcp.*
metrics (except connection state counts from network.tcpconn.*)
will report zero values.

.SS Manual Configuration
To enable TCP statistics manually, run as root:
.PP
.in +4n
.nf
$ sudo sysctl -w net.inet.tcp.disable_access_to_stats=0
.fi
.in
.PP
To make this permanent across reboots, add to
.BR /etc/sysctl.conf :
.PP
.in +4n
.nf
net.inet.tcp.disable_access_to_stats=0
.fi
.in
.SH FILES
.TP 5
.B $PCP_PMDAS_DIR/darwin/help
One line help text for each metric
.TP
.B $PCP_LOG_DIR/pmcd/darwin.log
Log file for error and diagnostic messages
.SH SEE ALSO
.BR pmcd (1),
.BR pminfo (1),
.BR sysctl (8)
```

3. **help** - Update metric documentation to mention requirement:

```
@ network.tcp CLUSTER_TCP
TCP protocol statistics from the macOS kernel.

Note: Requires net.inet.tcp.disable_access_to_stats=0
See pmdadarwin(1) for configuration.

@ network.tcp.activeopens
Count of active TCP connection attempts (SYN sent)
...
```

4. **GNUmakefile** - Add man page installation

**Testing:**
- Verify warning appears in `$PCP_LOG_DIR/pmcd/darwin.log` on systems with stats disabled
- Verify man page displays correctly: `man pmdadarwin`

---

#### Step 2.5c: Auto-Enable Configuration (Optional/Future)

**Goal:** Allow users to configure automatic enabling of TCP statistics via config file

**Status:** DEFERRED - For discussion with PCP maintainers

**Why Deferred:** This feature changes kernel settings automatically, which may be controversial. Better to get feedback from maintainers before implementing.

**Proposed Implementation** (when/if approved):

1. Create `src/pmdas/darwin/darwin.conf`:
   ```conf
   # darwin.conf - Configuration for darwin PMDA
   # auto_enable_tcp_stats = false
   ```

2. Add config loading and auto-enable in `pmda.c` (see earlier plan for code)

3. Update man page with auto-enable documentation

4. Update GNUmakefile to install config file

**Discussion Points for Maintainers:**
- Is it acceptable for PMDA to modify sysctl settings?
- Should this be opt-in (default false) or opt-out?
- Where should darwin.conf be installed? ($PCP_PMDAS_DIR/darwin or $PCP_SYSCONF_DIR/darwin?)

---

### Discovery: Access Control via Sysctl Flag (Background Research)

Research (2026-01-03) revealed that TCP statistics ARE available on macOS, but controlled by a flag:

**The `net.inet.tcp.disable_access_to_stats` Flag:**
- Default value: `1` (disabled) on macOS for privacy/security
- When set to `1`: `net.inet.tcp.stats` sysctl succeeds but returns all zeros
- When set to `0`: Full TCP statistics available (228 fields from `struct tcpstat`)
- **NOT SIP-protected** - can be changed as root
- This is NOT a permissions failure, it's a deliberate data zeroing mechanism

**Evidence:**
```bash
# With flag disabled (default):
$ sysctl net.inet.tcp.disable_access_to_stats
net.inet.tcp.disable_access_to_stats: 1
$ ./test_program
tcps_connattempt: 0  # All zeros!

# With flag enabled:
$ sudo sysctl -w net.inet.tcp.disable_access_to_stats=0
$ ./test_program
tcps_connattempt: 19374  # Real data!
tcps_sndtotal: 10781426
tcps_rcvtotal: 9957250
```

**How Other Tools Handle This:**

1. **Prometheus node_exporter**: Does NOT implement TCP stats on Darwin (Linux-only feature)
2. **Netdata**: DOES implement TCP stats via `net.inet.tcp.stats` sysctl but:
   - Documentation says "No action required"
   - Code does NOT check the flag
   - Silently reports zeros when flag is disabled (confusing!)

**Conclusion:** TCP statistics ARE available on macOS! Implementation broken into 3 manageable steps (see above).

**References:**
- Netdata implementation: [freebsd.plugin TCP stats](https://learn.netdata.cloud/docs/collecting-metrics/freebsd/net.inet.tcp.stats)
- tcpstat structure: `/Library/Developer/CommandLineTools/SDKs/MacOSX15.4.sdk/usr/include/netinet/tcp_var.h`
- Research discussion: This conversation

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
