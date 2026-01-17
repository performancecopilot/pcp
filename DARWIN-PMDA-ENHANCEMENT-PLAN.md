# Darwin PMDA Enhancement Plan

## Current Status

**Last Updated:** 2026-01-17
**Current Step:** Phase 3 complete, addressing PR #2442 review feedback before Phase 4
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
| 2.5-pre | COMPLETED | Enable TCP stats in Cirrus CI (commit 1d967ef777) |
| 2.5a | COMPLETED | TCP protocol statistics - basic implementation (commits 2bf73ecef5, 540e5304b2) |
| 2.5b | COMPLETED | TCP statistics - detection and documentation (warnings + man page) (commit f5c406e52a) |
| 2.5c | DEFERRED | TCP statistics - auto-enable config (for maintainer discussion) |
| 2.6 | COMPLETED | pmrep macOS monitoring views - memory, disk, TCP, protocol overview (commit 9afb1b9e9d + QA fixes 3a2e05e5da) |
| 3.1 | COMPLETED | Process I/O statistics (commit e0b925a347) |
| 3.2 | COMPLETED | Process file descriptor count (ready for commit) |
| 3B.1 | COMPLETED | PR Feedback: Copyright header updates (commit d9678a39a0) |
| 3B.2 | COMPLETED | PR Feedback: Test infrastructure relocation (commit 1f521b1ad1) |
| 3B.3 | COMPLETED | PR Feedback: TCP granular error metrics (commits 459ab13d36, 4cd20e6167) - Tested ✓ |
| 3B.4 | PENDING | PR Feedback: UDP granular error metrics |
| 3B.5 | PENDING | PR Feedback: TCP state validation logging |
| 4.1 | PENDING | Transform plan → permanent documentation |
| 4.2 | PENDING | Refactor pmda.c legacy code |

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
| `dev/darwin/test/unit/test-*.txt` | Unit tests (dbpmda) |
| `dev/darwin/test/integration/run-integration-tests.sh` | Integration tests |

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
- Create `dev/darwin/test/unit/test-memory-compression.txt` (see "Unit Test Pattern")
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

**Status:** COMPLETED

**Commits:** `2bf73ecef5`, `540e5304b2`

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

**Status:** COMPLETED

**Commit:** `f5c406e52a`

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

**IMPORTANT OPERATIONAL NOTE:**
The `net.inet.tcp.disable_access_to_stats` flag must be set **before PMCD starts** (or PMCD must be restarted after changing it). The PMDA reads the TCP statistics during initialization, and if the flag is disabled at startup, the sysctl call succeeds but returns all zeros. Simply enabling the flag after PMCD is running will NOT make metrics start working - a PMCD restart is required.

**Recommended workflow for users:**
1. Enable TCP stats: `sudo sysctl -w net.inet.tcp.disable_access_to_stats=0`
2. Make permanent: Add `net.inet.tcp.disable_access_to_stats=0` to `/etc/sysctl.conf`
3. Restart PMCD: `sudo launchctl stop <pmcd-label> && sudo launchctl start <pmcd-label>`
4. Verify: Check `/var/log/pcp/pmcd/darwin.log` for absence of TCP warning

---

### Step 2.6: pmrep macOS System Monitoring Views

**Goal:** Create comprehensive pmrep configurations for macOS system monitoring across memory, disk, and network subsystems

**Status:** COMPLETED

**Commit:** `9afb1b9e9d`

**Depends On:** Steps 2.1-2.5 complete (all networking metrics implemented)

**Rationale:**
- Phase 1 added memory compression and VFS metrics
- Phase 2 added extensive networking metrics (UDP, ICMP, sockets, TCP conn states, TCP stats)
- Disk metrics already exist and can be showcased better
- Current `:macstat` and `:macstat-x` views provide basic overview but lack specialized deep-dive views
- Network/memory/disk administrators would benefit from dedicated subsystem-focused views
- Provides out-of-the-box monitoring without custom pmrep configs

**Implementation Summary:**

Six pmrep monitoring views successfully added to `src/pmrep/conf/macstat.conf`:

1. **:macstat (enhanced)** - Added `network.interface.total.bytes` for integrated network overview
2. **:macstat-x (enhanced)** - Added VFS metrics (files, vnodes) and disk byte I/O for extended analysis
3. **:macstat-mem (new)** - Comprehensive memory deep-dive with:
   - Memory breakdown (physmem, used, free, wired, active, inactive, compressed)
   - Swap activity (pagesin/pagesout)
   - Compression efficiency (compressions, decompressions, compressor pages)
   - Paging activity (pageins, pageouts, faults, COW faults, zero-filled, reactivated)
   - Cache efficiency with derived cache hit ratio metric
4. **:macstat-dsk (new)** - Disk I/O deep-dive with:
   - IOPS metrics (r/s, w/s, tot/s)
   - Throughput metrics (rkB/s, wkB/s, tkB/s)
   - Block-level I/O (rblk/s, wblk/s, tblk/s)
   - Latency metrics (rms, wms, tms)
5. **:macstat-tcp (new)** - TCP connection monitoring with:
   - Connection activity (activeopens, passiveopens, established)
   - All 11 TCP connection states (SYN_SENT, SYN_RECV, FIN_WAIT_1/2, TIME_WAIT, etc.)
   - Error metrics (attemptfails, estabresets, retranssegs, inerrs, outrsts)
   - Throughput (insegs, outsegs)
   - Socket usage (tcpsock)
6. **:macstat-proto (new)** - Network protocol overview with:
   - Interface totals (bytes, packets, errors, drops)
   - UDP statistics (indatagrams, outdatagrams, noports, inerrors, rcvbuferrors)
   - ICMP statistics (inmsgs, outmsgs, inerrors, inechos, outechos)
   - TCP summary (insegs, outsegs, retranssegs, inerrs)
   - Socket usage (TCP and UDP)

**Code Quality:**
- All metric references verified against darwin PMDA pmns
- Configuration syntax validated for pmrep format compliance
- Abbreviation patterns follow PCP conventions (vmstat/iostat style)
- Section comments provide clear purpose for each view
- Derived metric (cache_hit_ratio) properly configured with correct unit (%)
- File header updated with usage examples for all six views

**Code Review Findings:**
- All critical issues identified and fixed
- Metric naming corrected (removed non-existent `.in.bytes`/`.out.bytes` paths)
- Derived metric identifier and unit corrected
- Documentation enhanced with view descriptions
- Redundant section headers consolidated
- Passes all PCP coding standards

**QA Feedback and Fixes (commit 3a2e05e5da):**

Post-implementation testing revealed three issues that were addressed:

1. **Bug: cache_hit_ratio type mismatch error**
   - Error: `Semantic error: derived metric cache_hit_ratio: mem.cache_lookups : 1: Different types for ternary operands`
   - Root cause: Ternary operator mixed PM_TYPE_U64 (`mem.cache_lookups`) with PM_TYPE_32 (literal `1`)
   - Fix: Restructured formula to return final value from both branches instead of divisor
   - Before: `100 * mem.cache_hits / (mem.cache_lookups > 0 ? mem.cache_lookups : 1)`
   - After: `mem.cache_lookups > 0 ? 100 * mem.cache_hits / mem.cache_lookups : 0`

2. **Layout: :macstat view too wide with per-interface columns**
   - Issue: `network.interface.total.bytes` expanded to one column per interface (lo0, gif0, stf0, XHC14, anpi0, en1, en0, utun0, utun1, utun2, utun3)
   - Fix: Replaced per-interface metrics with aggregated totals using `sum()` function
   - Added: `net_in` and `net_out` derived metrics using `sum(network.interface.in.bytes)` and `sum(network.interface.out.bytes)`
   - Result: Compact view showing total network in/out bandwidth across all interfaces

3. **Layout: :macstat-proto view extremely wide**
   - Issue: Same per-interface expansion made protocol view unreadable (40+ columns)
   - Fix: Added `colxrow = "     IFACE"` to display interfaces as rows instead of columns (similar to `sar -n DEV`)
   - Benefit: Users can filter specific interfaces with `pmrep -i en0 :macstat-proto`
   - Result: Readable output with each interface as a separate row

**Lessons Learned:**
- pmrep's `sum()` function aggregates metrics across all instances (discovered from `collectl.conf` examples)
- Metrics with instance domains expand to multiple columns unless aggregated or displayed as rows via `colxrow`
- Type compatibility in derived metrics requires both ternary branches to return same type
- QA testing with real output is essential for catching usability issues that unit/integration tests miss

---

#### Labeling Patterns (Based on Existing PCP Conventions)

**Research findings from existing pmrep configs (vmstat.conf, iostat.conf, sar.conf, macstat.conf):**

1. **Short abbreviations (2-8 chars)**: `swpd`, `free`, `wired`, `active`, `cmpr`, `inact`, `comp`, `deco`, `proc`, `thrd`
2. **Two-letter activity codes** (following vmstat pattern): `si`/`so` (swap), `pi`/`po` (page), `bi`/`bo` (block), `ni`/`no` (network)
3. **Read/write prefixes** (iostat pattern): `r/s`, `w/s`, `rkB/s`, `wkB/s`, `rtotal`, `wtotal`
4. **Grouping via structure, NOT prefixes**:
   - Use **comments** to separate logical sections
   - Use **adjacent placement** of related metrics
   - Use **consistent abbreviation style** within groups
   - **NO underscores** for grouping (avoid `mem_free`, `tcp_estab`)

**Visual grouping achieved through:**
- Section comment headers
- Consistent abbreviation patterns within sections
- Logical column ordering

---

#### Mode 1: :macstat (Revised Basic Overview)

**Purpose:** Quick system health overview - the "glance at the dashboard" view

**Implemented metrics:**
- Load: kernel.all.load
- Memory: swap.used, mem.util.{free, wired, active, compressed}
- Paging: mem.pageins, mem.pageouts
- Disk: disk.all.{read, write}
- Network: Aggregated totals across all interfaces
  - `net_in` (derived) = `sum(network.interface.in.bytes)` labeled as `netin`
  - `net_out` (derived) = `sum(network.interface.out.bytes)` labeled as `netout`
- CPU: user%, sys%, idle%

**Implementation notes:**
- Network metrics use pmrep's `sum()` function to aggregate across all interfaces
- Provides single-number bandwidth indicators without excessive column width
- Labels follow existing `pi`/`po` pattern for consistency

---

#### Mode 2: :macstat-x (Revised Extended Overview)

**Purpose:** Detailed system resource utilization for capacity planning

**Current metrics:**
- Everything from :macstat
- Additional memory: mem.util.inactive
- Compression: mem.compressions, mem.decompressions
- System resources: kernel.all.nprocs, kernel.all.nthreads

**Add VFS metrics:**
- `vfs.files.count` = `files`
- `vfs.files.max` = `fmax`
- `vfs.vnodes.count` = `vnodes`

**Add disk byte metrics:**
- `disk.all.read_bytes` = `rkB/s` (or similar, with KB unit scale)
- `disk.all.write_bytes` = `wkB/s`

**Changes Required:**
1. Modify existing `src/pmrep/conf/macstat.conf` [macstat-x] section
2. Add VFS metrics section (after process/thread counts)
3. Enhance disk I/O section with byte metrics
4. Adjust column widths as needed

**Rationale:** VFS exhaustion (file/vnode limits) is a real macOS issue; byte-level disk I/O provides bandwidth context

---

#### Mode 3: :macstat-mem (Memory Deep-Dive)

**Purpose:** Detailed memory subsystem analysis - diagnose memory pressure and compression efficiency

**File:** `src/pmrep/conf/macstat.conf` (add new [macstat-mem] section)

**Metrics organized by section:**

**Memory breakdown:**
- `mem.physmem` = `phys` (total physical memory)
- `mem.util.used` = `used`
- `mem.util.free` = `free`
- `mem.util.wired` = `wired`
- `mem.util.active` = `actv`
- `mem.util.inactive` = `inact`
- `mem.util.compressed` = `cmpr`

**Swap:**
- `swap.used` = `swpd`
- `swap.free` = `swfree`
- `swap.pagesin` = `si`
- `swap.pagesout` = `so`

**Compression efficiency:**
- `mem.compressions` = `comp`
- `mem.decompressions` = `deco`
- `mem.compressor.pages` = `cpages`
- `mem.compressor.uncompressed_pages` = `upages`

**Paging activity:**
- `mem.pageins` = `pi`
- `mem.pageouts` = `po`
- `mem.pages.faults` = `faults`
- `mem.pages.cow_faults` = `cow`
- `mem.pages.zero_filled` = `zero`
- `mem.pages.reactivated` = `react`

**Cache efficiency:**
- `mem.cache_lookups` = `lookups`
- `mem.cache_hits` = `hits`
- **Derived metric: cache hit ratio** = `hit%`
  - Formula: `100 * mem.cache_hits / mem.cache_lookups`
  - Type: Calculated in pmrep config (for now)
  - **Future consideration:** Discuss with PCP maintainers about adding native PMDA metric

**Note on cache metrics:**
- `mem.cache_hits` and `mem.cache_lookups` are object cache statistics from `vm_statistics64.hits` and `vm_statistics64.lookups`
- Cache hit ratio is mathematically valid: every lookup either hits or misses, so `hits/lookups` = hit rate

---

#### Mode 4: :macstat-dsk (Disk Deep-Dive)

**Purpose:** Comprehensive disk I/O analysis - latency, throughput, and operation counts

**File:** `src/pmrep/conf/macstat.conf` (add new [macstat-dsk] section)

**Metrics organized by section:**

**Operations (IOPS):**
- `disk.all.read` = `r/s`
- `disk.all.write` = `w/s`
- `disk.all.total` = `tot/s`

**Throughput (bandwidth):**
- `disk.all.read_bytes` = `rkB/s` (with KB unit scale)
- `disk.all.write_bytes` = `wkB/s`
- `disk.all.total_bytes` = `tkB/s`

**Block-level I/O:**
- `disk.all.blkread` = `rblk/s`
- `disk.all.blkwrite` = `wblk/s`
- `disk.all.blktotal` = `tblk/s`

**Latency (milliseconds):**
- `disk.all.read_time` = `rms`
- `disk.all.write_time` = `wms`
- `disk.all.total_time` = `tms`

**Rationale:** See both IOPS and bandwidth to understand if bottleneck is operations or throughput; latency metrics show performance

---

#### Mode 5: :macstat-tcp (TCP-Focused)

**Purpose:** TCP connection lifecycle and health monitoring

**File:** `src/pmrep/conf/macstat.conf` (add new [macstat-tcp] section)

**Metrics organized by section:**

**Connection activity:**
- `network.tcp.activeopens` = `actopn`
- `network.tcp.passiveopens` = `psvopn`
- `network.tcp.currestab` = `estab`

**Connection states (all 11 states):**
- `network.tcpconn.established` = `estab`
- `network.tcpconn.syn_sent` = `synst`
- `network.tcpconn.syn_recv` = `synrv`
- `network.tcpconn.fin_wait1` = `fin1`
- `network.tcpconn.fin_wait2` = `fin2`
- `network.tcpconn.time_wait` = `timew`
- `network.tcpconn.close` = `close`
- `network.tcpconn.close_wait` = `closew`
- `network.tcpconn.last_ack` = `lack`
- `network.tcpconn.listen` = `listn`
- `network.tcpconn.closing` = `closg`

**Errors and retransmissions:**
- `network.tcp.attemptfails` = `fails`
- `network.tcp.estabresets` = `resets`
- `network.tcp.retranssegs` = `retran`
- `network.tcp.inerrs` = `errs`
- `network.tcp.outrsts` = `rsts`

**Throughput:**
- `network.tcp.insegs` = `isgmts`
- `network.tcp.outsegs` = `osgmts`

**Socket usage:**
- `network.sockstat.tcp.inuse` = `tcpsock`

**Rationale:** TCP-focused view for connection troubleshooting, state analysis, and retransmission monitoring

---

#### Mode 6: :macstat-proto (Protocol-Focused)

**Purpose:** Network protocol overview - UDP, ICMP, interface stats, TCP summary

**File:** `src/pmrep/conf/macstat.conf` (add new [macstat-proto] section)

**Display format:** Uses `colxrow = "     IFACE"` to show each interface as a separate row (similar to `sar -n DEV`)

**Metrics organized by section:**

**Interface totals (per-interface rows):**
- `network.interface.total.bytes` = `bytes` (KB units)
- `network.interface.total.packets` = `pkts`
- `network.interface.total.errors` = `errs`
- `network.interface.total.drops` = `drops`

**User filtering:** Can filter specific interfaces with `pmrep -i en0 :macstat-proto` or `pmrep -i en0,en1 :macstat-proto`

**UDP:**
- `network.udp.indatagrams` = `udpin`
- `network.udp.outdatagrams` = `udpout`
- `network.udp.inerrors` = `udperr`
- `network.udp.noports` = `udpnop`
- `network.udp.rcvbuferrors` = `udpbuf`

**ICMP:**
- `network.icmp.inmsgs` = `icmpin`
- `network.icmp.outmsgs` = `icmpout`
- `network.icmp.inechos` = `iecho`
- `network.icmp.outechos` = `oecho`
- `network.icmp.inerrors` = `ierr`

**TCP summary:**
- `network.tcp.insegs` = `tcpin`
- `network.tcp.outsegs` = `tcpout`
- `network.tcp.retranssegs` = `tcpret`
- `network.tcp.inerrs` = `tcperr`

**Socket usage:**
- `network.sockstat.tcp.inuse` = `tcpsock`
- `network.sockstat.udp.inuse` = `udpsock`

**Rationale:** Protocol-level overview for network troubleshooting - see all protocols at once

---

#### Implementation Notes

**File location:** All modes in single file: `src/pmrep/conf/macstat.conf`
- Modify existing [macstat] and [macstat-x] sections
- Add new sections: [macstat-mem], [macstat-dsk], [macstat-tcp], [macstat-proto]

**pmrep configuration format:**
```
[section-name]
header = yes
unitinfo = no
globals = no
timestamp = no  # or yes for timestamped output
precision = 0
delimiter = " "
repeat_header = auto

# Section comment for visual grouping
metric.name.path = label,,unit/scale,,width

# Derived metrics (if needed)
derived_name = source.metric.name
derived_name.label = label
derived_name.formula = expression
derived_name.unit = unit
derived_name.width = width
```

**Testing:**
1. Verify each config section loads without errors
2. Test all metrics are fetchable on macOS
3. Validate output formatting and column widths
4. Test with `pmrep :macstat-mem`, `pmrep :macstat-tcp`, etc.
5. Ensure column alignment and readability

**Documentation:**
- Update `pmdadarwin(1)` man page with usage examples
- Document all new views with purpose and example output
- Include in PMDA documentation or README

**Future Consideration:**
- Discuss with PCP maintainers about adding native calculated metrics in PMDA (e.g., cache hit ratio, compression ratio)
- Consider whether some derived metrics should move from pmrep config to PMDA fetch functions

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

**Status:** COMPLETED

**Goal:** Add per-process disk I/O statistics via `proc_pid_rusage()` API

**New Cluster:** `CLUSTER_PROC_IO` (5)

**New Metrics:**

| Metric | Item | Type | Semantics | Source |
|--------|------|------|-----------|--------|
| `proc.io.read_bytes` | 0 | U64 | counter (bytes) | rusage_info_v3.ri_diskio_bytesread |
| `proc.io.write_bytes` | 1 | U64 | counter (bytes) | rusage_info_v3.ri_diskio_byteswritten |

**Changes Made:**

1. **kinfo_proc.h** - Added I/O fields to `darwin_proc_t` structure:
   - `uint64_t read_bytes` - cumulative bytes read from disk
   - `uint64_t write_bytes` - cumulative bytes written to disk

2. **kinfo_proc.c** - Added data collection in `darwin_refresh_processes()`:
   ```c
   struct rusage_info_v3 rusage;
   if (proc_pid_rusage(proc->id, RUSAGE_INFO_V3,
           (rusage_info_t *)&rusage) == 0) {
       proc->read_bytes = rusage.ri_diskio_bytesread;
       proc->write_bytes = rusage.ri_diskio_byteswritten;
   } else {
       proc->read_bytes = 0;
       proc->write_bytes = 0;
   }
   ```

3. **pmda.c** - Added cluster and metrics:
   - Added `CLUSTER_PROC_IO` to cluster enum
   - Added 2 metrictab entries for read_bytes and write_bytes
   - Added cluster to `proc_refresh()` condition
   - Added cluster to `proc_instance()` refresh list
   - Added `CLUSTER_PROC_IO` case in `proc_fetchCallBack()` for fetching I/O values

4. **root_proc** - Added PMNS entries:
   - Added `io` to proc{} hierarchy
   - Created `proc.io{}` section with read_bytes and write_bytes mappings

5. **help** - Added comprehensive help text:
   - Documented both metrics as cumulative counters
   - Noted data source (proc_pid_rusage)

**Implementation Notes:**
- Metrics are PM_SEM_COUNTER (cumulative since process start)
- Metrics use PM_TYPE_U64 to handle large I/O volumes
- Zero-initialization on rusage failure prevents stale data
- Follows exact pattern of CLUSTER_PROC_MEM for consistency
- Permission checks inherited from existing access control

**Code Review:**
- pcp-code-reviewer verdict: **APPROVED FOR MERGE**
- Ratings: Excellent across all categories
- No blocking issues found
- Production-ready code quality

**Testing:**
- Unit tests added to `dev/darwin/test/unit/test-proc.txt`:
  - Tests proc.io.read_bytes and proc.io.write_bytes
  - Validates metric descriptions and fetchability
- Integration tests added to `dev/darwin/test/integration/run-integration-tests.sh`:
  - Test Group 16: Process I/O statistics
  - Validates metrics exist and are fetchable
  - Note: Cannot validate specific values as processes may have no I/O yet
- Tests account for dynamic process list and varying I/O activity

---

### Step 3.2: Enhanced Process Metrics

**Status:** COMPLETED (proc.fd.count only)

**Goal:** Add enhanced per-process metrics for file descriptors and memory

**New Cluster:** `CLUSTER_PROC_FD` (6)

**New Metrics:**

| Metric | Item | Type | Semantics | Source |
|--------|------|------|-----------|--------|
| `proc.fd.count` | 0 | U32 | instant (count) | proc_pidinfo(PROC_PIDLISTFDS) |

**Changes Made:**

1. **kinfo_proc.h** - Added FD count field to `darwin_proc_t` structure:
   - `uint32_t fd_count` - instantaneous count of open file descriptors

2. **kinfo_proc.c** - Added data collection in `darwin_process_set_taskinfo()`:
   ```c
   /* file descriptor count */
   {
       int bufsize = proc_pidinfo(proc->id, PROC_PIDLISTFDS, 0, NULL, 0);

       if (bufsize > 0) {
           proc->fd_count = bufsize / sizeof(struct proc_fdinfo);
       } else {
           proc->fd_count = 0;
       }
   }
   ```

3. **pmda.c** - Added cluster, metric definition, and fetch logic:
   - Added `CLUSTER_PROC_FD` (6) to cluster enum
   - Added metrictab entry for `proc.fd.count` (PM_TYPE_U32, PM_SEM_INSTANT)
   - Added fetch callback case in `proc_fetchCallBack()` with access control
   - Updated `proc_refresh()` to include CLUSTER_PROC_FD
   - Updated `proc_instance()` to include CLUSTER_PROC_FD

4. **root_proc** - Added PMNS namespace:
   - Created `proc.fd` namespace
   - Added `proc.fd.count` metric mapping (PROC:6:0)

5. **help** - Added metric documentation:
   - Help text explaining the metric measures instantaneous FD count
   - Documents the source as `proc_pidinfo(PROC_PIDLISTFDS)`

**Implementation Notes:**
- Uses `proc_pidinfo(PROC_PIDLISTFDS)` to query buffer size needed for FD list
- FD count calculated by dividing buffer size by `sizeof(struct proc_fdinfo)`
- Fixed potential portability issue: uses `sizeof()` instead of `PROC_PIDLISTFD_SIZE` macro
- Access control implemented via existing `have_access` pattern (same as other proc metrics)
- Returns 0 if proc_pidinfo fails or process has no permission
- Type U32 chosen as sufficient for file descriptor counts (max open files typically < 4 billion)

**Testing:**
- Unit tests added to `dev/darwin/test/unit/test-proc.txt`:
  - Tests proc.fd.count metric existence and fetchability
  - Tests proc.io.read_bytes and proc.io.write_bytes (Step 3.1)
  - Tests basic proc metrics (nprocs, psinfo, memory, runq)
- Integration tests added to `dev/darwin/test/integration/run-integration-tests.sh`:
  - Test Group 15: Basic process metrics (proc.nprocs, psinfo, memory)
  - Test Group 16: Process I/O statistics (Step 3.1)
  - Test Group 17: Process FD count (Step 3.2) - validates at least one process has FDs > 0
- All tests validate metric existence and fetchability
- Integration tests account for dynamic process list (don't test specific values)

**Note:** `proc.memory.vmsize` metric deferred - memory size already available via `proc.memory.size` (virtual size) and `proc.memory.rss` (resident size) from existing implementation.

---

## Phase 3B: Address PR #2442 Review Feedback

**Context:** Ken McDonell reviewed PR #2442 and provided feedback requiring changes before merge. This phase addresses all review comments.

**PR Review:** https://github.com/performancecopilot/pcp/pull/2442#pullrequestreview-3668265761

**Key Feedback Items:**
1. Copyright attribution - Add contributor name to new files
2. Test infrastructure location - Move to top-level for discoverability
3. TCP/UDP error metrics - Add granular breakdown for diagnostics
4. TCP state validation - Add logging for unexpected states

**Testing Strategy:**
- Phases 3B.1-3B.2: No testing needed (non-functional changes)
- Phase 3B.3: Run full test suite after TCP metrics implementation
- Phases 3B.4-3B.5: Verify with existing test suite
- Use Cirrus VM for all testing (local testing not available)

---

### Step 3B.1: Copyright Header Updates

**Status:** COMPLETED (commit d9678a39a0)

**Goal:** Add "Paul Smith" to copyright headers in all new/modified darwin PMDA files per PCP project standards.

**Files to Update** (12 total):

| File | Current Copyright | Required Change |
|------|------------------|-----------------|
| `src/pmdas/darwin/tcp.c` | `Copyright (c) 2026 Red Hat.` | Add `, Paul Smith` |
| `src/pmdas/darwin/tcp.h` | `Copyright (c) 2026 Red Hat.` | Add `, Paul Smith` |
| `src/pmdas/darwin/udp.c` | `Copyright (c) 2026 Red Hat.` | Add `, Paul Smith` |
| `src/pmdas/darwin/udp.h` | `Copyright (c) 2026 Red Hat.` | Add `, Paul Smith` |
| `src/pmdas/darwin/icmp.c` | `Copyright (c) 2026 Red Hat.` | Add `, Paul Smith` |
| `src/pmdas/darwin/icmp.h` | `Copyright (c) 2026 Red Hat.` | Add `, Paul Smith` |
| `src/pmdas/darwin/sockstat.c` | `Copyright (c) 2026 Red Hat.` | Add `, Paul Smith` |
| `src/pmdas/darwin/sockstat.h` | `Copyright (c) 2026 Red Hat.` | Add `, Paul Smith` |
| `src/pmdas/darwin/tcpconn.c` | `Copyright (c) 2024 Red Hat.` | Update to 2026, add `, Paul Smith` |
| `src/pmdas/darwin/tcpconn.h` | `Copyright (c) 2024 Red Hat.` | Update to 2026, add `, Paul Smith` |
| `src/pmdas/darwin/vfs.c` | `Copyright (c) 2025 Red Hat.` | Update to 2026, add `, Paul Smith` |
| `src/pmdas/darwin/vfs.h` | `Copyright (c) 2025 Red Hat.` | Update to 2026, add `, Paul Smith` |

**Pattern:**
```c
// BEFORE:
 * Copyright (c) YYYY Red Hat.

// AFTER:
 * Copyright (c) 2026 Red Hat, Paul Smith.
```

**Changes Required:**
- Edit copyright line in each file header
- No functional code changes

**Testing:** None required (non-functional change)

**Commit Message:** "Update copyright headers for Darwin PMDA contributors"

---

### Step 3B.2: Test Infrastructure Relocation

**Status:** COMPLETED (commit 1f521b1ad1)

**Goal:** Move `dev/darwin/` to `dev/darwin/` for better discoverability and maintainability.

**Current Structure:**
```
dev/darwin/
├── README.md
├── .gitignore
├── dev/                    # Build environment
└── test/                   # Test infrastructure
    ├── unit/              # 13 unit test files
    └── integration/       # Integration test scripts
```

**Target Structure:**
```
dev/darwin/
├── README.md
├── .gitignore
├── dev/                    # Build environment
└── test/                   # Test infrastructure
    ├── unit/              # 13 unit test files
    └── integration/       # Integration test scripts
```

**Changes Required:**
1. Create top-level `dev/` directory if needed
2. Move `dev/darwin/` → `dev/darwin/`
3. Update path references in:
   - Documentation (README.md, this plan)
   - `.cirrus.yml` lines 69 and 72: `dev/darwin/test/` → `dev/darwin/test/`

**Testing:** Verify build and test scripts still work after move:
```bash
dev/darwin/dev/build-quick.sh
dev/darwin/test/quick-test.sh
```

**Commit Message:** "Relocate darwin PMDA development infrastructure to dev/"

---

### Step 3B.3: TCP Granular Error Metrics

**Status:** COMPLETED (commits 459ab13d36, 4cd20e6167) - Fully tested ✓

**Goal:** Add hierarchical TCP error metrics for diagnostic visibility into individual error types.

**Current Implementation:**
- Single aggregate: `network.tcp.inerrs` (item 176)
- Computed as sum of 4 error types in `fetch_tcp()`

**Target Implementation:**
- Hierarchical namespace with 5 metrics (1 aggregate + 4 individual)

**Item Number Allocation:**
✅ **Verified safe** - TCP cluster 17 currently uses items 168-182, new items 183-186 available

**New PMNS Structure** (`src/pmdas/darwin/pmns`):
```
network.tcp.inerrs {
    total       DARWIN:17:176    # Aggregate (backward compat)
    badsum      DARWIN:17:183    # Bad checksum errors
    badoff      DARWIN:17:184    # Bad offset errors
    short       DARWIN:17:185    # Truncated packets
    memdrop     DARWIN:17:186    # Memory exhaustion drops
}
```

**Metric Mappings:**

| Metric | Item | Type | Source | Description |
|--------|------|------|--------|-------------|
| `network.tcp.inerrs.total` | 176 | U64 | computed | Total TCP input errors |
| `network.tcp.inerrs.badsum` | 183 | U64 | `tcps_rcvbadsum` | Bad checksum (NIC/transmission issues) |
| `network.tcp.inerrs.badoff` | 184 | U64 | `tcps_rcvbadoff` | Bad offset (malformed packets) |
| `network.tcp.inerrs.short` | 185 | U64 | `tcps_rcvshort` | Truncated (MTU mismatches) |
| `network.tcp.inerrs.memdrop` | 186 | U64 | `tcps_rcvmemdrop` | Memory exhaustion |

**Code Changes:**

1. **src/pmdas/darwin/pmns** - Replace single `inerrs` line with hierarchy block

2. **src/pmdas/darwin/metrics.c** - Add 4 new metrictab entries (items 183-186):
   ```c
   /* network.tcp.inerrs.total - keep existing */
   { NULL, { PMDA_PMID(CLUSTER_TCP,176), ... }, },

   /* network.tcp.inerrs.badsum - direct pointer */
   { &mach_tcp.stats.tcps_rcvbadsum,
     { PMDA_PMID(CLUSTER_TCP,183), PM_TYPE_U64, PM_INDOM_NULL,
       PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

   /* ...similar for badoff (184), short (185), memdrop (186) */
   ```

3. **src/pmdas/darwin/tcp.c** - Update `fetch_tcp()` comment for item 176

4. **src/pmdas/darwin/help** - Add help text for 5 metrics with diagnostic context

5. **dev/darwin/test/unit/test-tcp.txt** - Add tests for all 5 metrics

6. **dev/darwin/test/integration/run-integration-tests.sh** - Add test group

**Diagnostic Value:**
- **badsum**: Hardware/transmission problems
- **badoff**: Malformed packets, potential attacks
- **short**: MTU mismatches, packet fragmentation issues
- **memdrop**: System resource pressure

**Testing:** ✅ Full test suite passed via Makepkgs build

**Implementation Notes:**
- Commit 459ab13d36: Initial TCP granular error metrics implementation
- Commit 4cd20e6167: Fixed help file build error (removed non-leaf node entry for network.tcp.inerrs parent)
- All build and QA tests pass successfully

**Commit Messages:**
- "Add granular TCP input error metrics for diagnostics"
- "Fix darwin PMDA build error with help file"

---

### Step 3B.4: UDP Granular Error Metrics

**Status:** PENDING

**Goal:** Add hierarchical UDP error metrics (same pattern as TCP).

**Current Implementation:**
- Single aggregate: `network.udp.inerrors` (item 145)
- Computed as sum of 3 error types in `fetch_udp()`

**Target Implementation:**
- Hierarchical namespace with 4 metrics (1 aggregate + 3 individual)

**Item Number Allocation:**
✅ **Verified safe** - UDP cluster 13 currently uses items 142-146, new items 147-149 available
- Note: Item numbers are cluster-specific (PMID = DARWIN:cluster:item)
- UDP cluster 13 item 147 does NOT conflict with ICMP cluster 14 item 147

**New PMNS Structure** (`src/pmdas/darwin/pmns`):
```
network.udp.inerrors {
    total       DARWIN:13:145    # Aggregate
    hdrops      DARWIN:13:147    # Header drops
    badsum      DARWIN:13:148    # Bad checksum
    badlen      DARWIN:13:149    # Bad length
}
```

**Metric Mappings:**

| Metric | Item | Type | Source | Description |
|--------|------|------|--------|-------------|
| `network.udp.inerrors.total` | 145 | U64 | computed | Total UDP input errors |
| `network.udp.inerrors.hdrops` | 147 | U64 | `hdrops` | Header error drops |
| `network.udp.inerrors.badsum` | 148 | U64 | `badsum` | Bad checksum |
| `network.udp.inerrors.badlen` | 149 | U64 | `badlen` | Bad length |

**Code Changes:**

1. **src/pmdas/darwin/pmns** - Replace single `inerrors` line with hierarchy block

2. **src/pmdas/darwin/metrics.c** - Add 3 new metrictab entries (items 147-149):
   ```c
   /* network.udp.inerrors.total - keep existing */
   { NULL, { PMDA_PMID(CLUSTER_UDP,145), ... }, },

   /* Item 146 is rcvbuferrors - keep */

   /* network.udp.inerrors.hdrops - direct pointer */
   { &mach_udp.hdrops,
     { PMDA_PMID(CLUSTER_UDP,147), PM_TYPE_U64, PM_INDOM_NULL,
       PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

   /* ...similar for badsum (148), badlen (149) */
   ```

3. **src/pmdas/darwin/udp.c** - Update `fetch_udp()` comment for item 145

4. **src/pmdas/darwin/help** - Add help text for 4 metrics

5. **dev/darwin/test/unit/test-udp.txt** - Add tests for all 4 metrics

6. **dev/darwin/test/integration/run-integration-tests.sh** - Add test group

**Testing:** Run test suite via Cirrus VM

**Commit Message:** "Add granular UDP input error metrics for diagnostics"

---

### Step 3B.5: TCP Connection State Validation Enhancement

**Status:** PENDING

**Goal:** Add diagnostic logging for out-of-range TCP connection states with one-trip guard.

**Current Implementation** (`src/pmdas/darwin/tcpconn.c`, lines 76-78):
```c
/* Validate state and increment counter */
if (tp->t_state >= 0 && tp->t_state < TCP_NSTATES)
    stats->state[tp->t_state]++;
/* Out-of-range states silently skipped */
```

**Target Implementation:**
- Add `pmNotifyErr()` logging for first invalid state occurrence
- Use one-trip guard to prevent log flooding
- Do NOT aggregate unknown states (avoid metric pollution)

**Code Changes** (`src/pmdas/darwin/tcpconn.c`):

```c
/* Add at top of refresh_tcpconn() function */
static int warned_invalid_state = 0;

/* ... existing code ... */

/* Enhanced validation (replace lines 76-78) */
if (tp->t_state >= 0 && tp->t_state < TCP_NSTATES) {
    stats->state[tp->t_state]++;
} else if (!warned_invalid_state) {
    /* One-trip guard: log once then suppress */
    pmNotifyErr(LOG_WARNING,
        "tcpconn: unexpected TCP state %d (expected 0-%d), ignoring connection. "
        "This may indicate a kernel version mismatch. "
        "(Further invalid states will be silently ignored)",
        tp->t_state, TCP_NSTATES - 1);
    warned_invalid_state = 1;
}
/* Invalid states are NOT counted - dropped to prevent metric pollution */
```

**Rationale:**
- **pmNotifyErr vs pmDebug**: Ken McDonell recommended pmNotifyErr for visibility in logs
- **One-trip guard**: Prevents log flooding if many connections have invalid states
- **No aggregation**: Invalid states not counted to avoid polluting existing metrics
- **Diagnostic message**: Explains issue and that further occurrences won't be logged

**Testing:**
- No unit test (requires kernel with unexpected states)
- Manual verification: Check `/var/log/pcp/pmcd/darwin.log` after deployment

**Commit Message:** "Add diagnostic logging for invalid TCP connection states"

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

### Step 4.2: Refactor pmda.c Legacy Code

**Goal:** Extract legacy fetch functions from pmda.c into dedicated subsystem files

**Status:** PENDING

**Rationale:**
- pmda.c has grown to over 2000+ lines with embedded fetch logic from earlier development
- New subsystems (vfs, udp, icmp, sockstat, tcpconn, tcp) already follow modular pattern with dedicated .c/.h files
- Older code should be refactored to match this cleaner architecture
- Improves maintainability, testability, and code review
- Reduces pmda.c to primarily coordination/dispatch logic

**Approach:**

1. **Audit pmda.c** - Identify fetch functions that should move to existing subsystem files:
   - Disk-related fetch logic → move to `disk.c`
   - Network interface fetch logic → move to `network.c`
   - CPU/processor fetch logic → potentially extract to `cpu.c`
   - Any other domain-specific logic embedded in pmda.c

2. **Refactoring Pattern** (for each subsystem):
   - Move `fetch_<subsystem>()` function from pmda.c to subsystem.c
   - Update subsystem.h with function declarations
   - Keep only metrictab entries and dispatch logic in pmda.c
   - Ensure `extern` declarations are properly used

3. **Example - Disk subsystem:**
   ```c
   // BEFORE: fetch_disk() embedded in pmda.c

   // AFTER:
   // - disk.h: extern int fetch_disk(unsigned int, unsigned int, pmAtomValue *);
   // - disk.c: int fetch_disk(...) { /* implementation */ }
   // - pmda.c: case CLUSTER_DISK: return fetch_disk(item, inst, atom);
   ```

4. **Constraints:**
   - NO functional changes - pure code reorganization
   - All existing tests must continue to pass
   - Maintain binary compatibility (no ABI changes)
   - Keep commit history clean with clear refactoring commits

5. **Testing:**
   - Run full test suite after each subsystem refactoring
   - Verify no behavioral changes with integration tests
   - Use `macos-darwin-pmda-qa` agent to validate

6. **Benefits:**
   - pmda.c becomes smaller, focused on coordination
   - Each subsystem fully self-contained
   - Easier to understand and modify individual subsystems
   - Matches modern PCP PMDA architecture patterns

**Candidates for Extraction** (to be determined during audit):
- `fetch_disk()` - disk I/O metrics
- `fetch_network()` - network interface metrics
- Any fetch logic for cpu, hinv (hardware inventory), or other clusters
- Helper functions that are subsystem-specific

**Documentation:**
- Update "Code Organization Pattern" section with lessons learned
- Note which subsystems were refactored and why
- Provide guidance for future contributors on modular design

---

## Phase 5: Future Enhancements (DEFERRED)

### Step 5.1: Granular TCP/UDP Error Metrics (PR #2442 Review Feedback)

**Status:** PROPOSED - Based on Ken McDonell's review feedback

**Context:** PR #2442 review (https://github.com/performancecopilot/pcp/pull/2442#pullrequestreview-3668265761) raised the question of whether individual error metrics should be exported in addition to aggregate metrics.

**Current State:**
- **TCP:** `network.tcp.inerrs` aggregates: tcps_rcvbadsum + tcps_rcvbadoff + tcps_rcvshort + tcps_rcvmemdrop
- **UDP:** `network.udp.inerrors` aggregates: hdrops + badsum + badlen

**Rationale for Granular Metrics:**
Different error types have different diagnostic implications:
- **tcps_rcvbadsum** (bad checksums) → NIC issues, transmission line noise
- **tcps_rcvbadoff** (bad offset) → malformed packets, potential attack patterns
- **tcps_rcvshort** (truncated packets) → MTU mismatches, packet loss
- **tcps_rcvmemdrop** (buffer exhaustion) → system resource pressure
- **UDP errors** similarly provide distinct diagnostic value

**Proposed New Metrics:**

**TCP error breakdown** (4 new metrics):
- `network.tcp.inerrs.badsum` → tcps_rcvbadsum
- `network.tcp.inerrs.badoff` → tcps_rcvbadoff
- `network.tcp.inerrs.short` → tcps_rcvshort
- `network.tcp.inerrs.memdrop` → tcps_rcvmemdrop

**UDP error breakdown** (3 new metrics):
- `network.udp.inerrors.hdrops` → hdrops
- `network.udp.inerrors.badsum` → badsum
- `network.udp.inerrors.badlen` → badlen

**Implementation:**
1. Check Linux PMDA for consistency (do they export granular error metrics?)
2. Add new metrictab entries for each granular metric
3. Update PMNS with hierarchical error namespace
4. Update help text with diagnostic guidance
5. Keep aggregate metrics for backward compatibility

**Discussion Points:**
- Does this align with PCP philosophy of providing what the kernel provides?
- Should these be documented as "expert-level" metrics?
- Verify Linux PMDA approach for cross-platform consistency

---

### Step 5.2: TCP Connection State Defensive Validation (PR #2442 Review Feedback)

**Status:** PROPOSED - Based on Ken McDonell's review feedback

**Context:** Ken suggested adding defensive diagnostics for out-of-range TCP state values, as kernel enum values can advance without PMDA updates.

**Goal:** Add validation to detect when macOS kernel introduces new TCP states that the PMDA doesn't know about yet.

**Implementation:**

In `tcpconn.c`'s `refresh_tcpconn()`:

```c
/* Validate state is within expected range (TCPS_CLOSED=0 to TCPS_CLOSING=11) */
if (so.so_type == SOCK_STREAM) {
    if (xt.xt_tp.t_state > TCPS_CLOSING) {
        pmNotifyErr(LOG_WARNING,
            "tcpconn: unexpected TCP state %d (expected 0-%d), treating as CLOSING",
            xt.xt_tp.t_state, TCPS_CLOSING);
        tcp->state[TCPS_CLOSING]++;  /* Fail-safe: count as CLOSING */
    } else {
        tcp->state[xt.xt_tp.t_state]++;
    }
}
```

**Alternative:** Use `pmDebug(DBG_TRACE_APPL0, ...)` instead of `pmNotifyErr` for lower-priority logging

**Discussion Point:** Which logging approach is preferred for this scenario?

---

### Step 5.3: Test Infrastructure Generalization (PR #2442 Review Feedback)

**Status:** PROPOSED - Based on Ken McDonell's review feedback

**Context:** Ken noted that the `dev/darwin/test/integration/run-integration-tests.sh` was easily adaptable to Ubuntu with only minor changes, suggesting the test infrastructure could be generalized for other platforms.

**Contributor perspective (psmith):**
> "I do think it's possible we could consider making the unit/integration tests we've developed more general purpose... Your points about the differences between the Linux and macOS installation are still valid though, so it might actually make things trickier."

**Current Platform-Specific Code:**
- PMDA installation path resolution (Linux vs macOS paths)
- PMCD startup mechanism (`systemctl`/`service` vs `launchctl`)
- PMDA discovery in pmcd status

**Proposed Approach:**

1. **Evaluate feasibility:** Determine which test logic is truly platform-neutral vs platform-specific
2. **Create shared framework:**
   - Move core test functions (run_test, validate_metric, etc.) to `scripts/common/` or `qa/common/`
   - Keep platform-specific drivers in platform directories
3. **Platform abstraction layer:**
   - Create platform detection utilities
   - Abstract PMDA path resolution
   - Abstract PMCD service management
4. **Benefits:**
   - All platform PMDAs can benefit from standardized test framework
   - Reduces duplication across platform-specific test suites
   - Improves test coverage consistency

**Discussion Points:**
- Is this worth the complexity trade-off?
- Should this be part of a broader QA system refactoring (see Step 5.4)?
- Where should generalized test infrastructure live (scripts/common vs qa/)?

---

### Step 5.4: Full PCP QA System Port to macOS (PR #2442 Review Feedback)

**Status:** PROPOSED - Based on Ken McDonell's review feedback and ongoing work

**Context:** Ken asked why the full PCP QA system is "currently difficult/impossible to run on macOS" given successful implementations on *BSD and OpenIndiana.

**Current Barriers:**

1. **Architecture Naming Mismatch (Critical):**
   - macOS uses `arm64`, Linux uses `aarch64` for the same 64-bit ARM architecture
   - QA system can't locate package lists due to this mismatch
   - **Work in progress:** PR #2431 ("Fix local CI: Dynamic task lists and aarch64 support")
   - Status: Proven more complex than expected, deferred while focusing on Darwin PMDA

2. **Platform-Specific Assumptions:**
   - QA scripts assume Linux-centric paths and utilities
   - macOS deployment differs significantly (`/usr/local/lib/pcp` vs system defaults)
   - Some QA tests reference Linux kernel interfaces

3. **Container Build Issues:**
   - Git repository detection failures in container builds
   - Package resolution for Ubuntu 24.04 on ARM64 systems
   - Dynamic task discovery for reproduce command

**Proposed Path Forward:**

1. **Phase 1 (Critical):** Complete PR #2431 work
   - Resolve aarch64/arm64 architecture naming
   - Fix dynamic task discovery
   - Fix git repository detection in containers

2. **Phase 2 (Platform Adaptation):**
   - Add platform detection to QA scripts
   - Create Darwin-specific test groups (similar to Linux-specific tests)
   - Document macOS deployment paths and considerations

3. **Phase 3 (Integration):**
   - Integrate existing `dev/darwin/test/` framework with broader QA system
   - Determine which tests should be platform-neutral vs platform-specific
   - Create guidelines for future platform PMDA QA contributions

**Contributor perspective (psmith):**
> "We actually started a draft PR with trying to address this change (see #2431) but it's proven a tricky problem and we haven't got back to looking in to it (focused on the Darwin PMDA)."

**Discussion Points:**
- Should PR #2431 be prioritized and completed?
- Is a full QA system port worth the effort compared to maintaining platform-specific test suites?
- Can *BSD QA implementation serve as a reference model?

---

### Step 5.5: Auto-Enable TCP Stats Configuration

**Status:** DEFERRED - For discussion with PCP maintainers in subsequent PR

**Why Deferred:** This feature changes kernel settings automatically, which may be controversial. Better to get feedback from maintainers before implementing.

**Goal:** Allow users to configure automatic enabling of TCP statistics via config file

**Proposed Implementation** (when/if approved):

1. Create `src/pmdas/darwin/darwin.conf`:
   ```conf
   # darwin.conf - Configuration for darwin PMDA
   # auto_enable_tcp_stats = false
   ```

2. Add config loading and auto-enable in `pmda.c` (see Step 2.5c details earlier in plan)

3. Update man page with auto-enable documentation

4. Update GNUmakefile to install config file

**Discussion Points for Maintainers:**
- Is it acceptable for PMDA to modify sysctl settings?
- Should this be opt-in (default false) or opt-out?
- Where should darwin.conf be installed? ($PCP_PMDAS_DIR/darwin or $PCP_SYSCONF_DIR/darwin?)

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

### Code Style Requirements

**CRITICAL:** The darwin PMDA uses **TABS for indentation**, not spaces.

**Indentation Rules:**
- Use **tabs** (not spaces) for all indentation levels
- Function bodies: tab-indented
- Case statements: tab-indented after switch
- Multi-line continuations: tabs + alignment spaces
- NO 4-space or 2-space indentation anywhere

**Verification:**
```bash
# Check for improper space indentation (should return nothing)
grep -n "^    " src/pmdas/darwin/<file>.c

# Compare with existing files (should show tabs)
sed -n '25,35p' src/pmdas/darwin/udp.c | sed 's/\t/<TAB>/g'
```

**Example - CORRECT indentation:**
```c
int
refresh_example(example_t *data)
{
<TAB>size_t size = sizeof(data->stats);

<TAB>if (sysctlbyname("kern.example", &data->stats, &size, NULL, 0) == -1)
<TAB><TAB>return -oserror();

<TAB>return 0;
}
```

**Lesson Learned:**
- TCP implementation (commit 2bf73ecef5) initially used 4-space indentation (fixed in commit 540e5304b2)
- Always verify tab usage before committing - reviewers will catch this but fix it proactively

---

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
4. **Fix ALL issues identified** - including minor ones (formatting, style, documentation)
5. Re-test if code was modified
6. Commit after approval

**IMPORTANT:** Do not ignore "Minor" suggestions from code reviewer:
- Minor issues accumulate and affect code quality
- Style issues (indentation, formatting) should be fixed immediately
- Documentation improvements should be applied
- "Minor" doesn't mean "optional" - it means "not blocking but should fix"

**Lessons Learned:**
- VFS implementation initially used error accumulation (fixed in commit d431bae047)
- TCP implementation initially used 4-space indentation instead of tabs (fixed in commit 540e5304b2)
- Code reviewer correctly identified both anti-patterns
- Always validate error handling matches darwin PMDA conventions
- Always fix style issues immediately, don't defer them

---

## Test Patterns and Test Running

### Unit Test Pattern

Unit tests are dbpmda command files located in `dev/darwin/test/unit/test-*.txt`.

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
1. Create `dev/darwin/test/unit/test-<feature>.txt`
2. Add `desc <metric>` and `fetch <metric>` for each new metric
3. Include comments explaining what the metrics test
4. Follow the pattern in existing test files like `test-basic.txt` and `test-memory-compression.txt`

### Integration Test Pattern

Integration tests validate metrics through real PCP tools (pminfo, pmval, pmstat).

**Location**: `dev/darwin/test/integration/run-integration-tests.sh`

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
1. Open `dev/darwin/test/integration/run-integration-tests.sh`
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

---

## Things to Note and Review with Maintainers

### 1. TCP Statistics Access Control Behavior

**Issue:** The `net.inet.tcp.disable_access_to_stats` sysctl flag controls access to TCP protocol statistics on macOS. When disabled (value=1, the default), the `sysctlbyname("net.inet.tcp.stats")` call **succeeds** but returns all zeros.

**Critical Operational Requirement:** The flag must be set **before PMCD starts**, or PMCD must be restarted after enabling it. Simply changing the flag while PMCD is running will not make metrics start working.

**Current Implementation:**
- PMDA checks flag at startup and logs a warning if disabled
- Man page documents how to enable permanently via `/etc/sysctl.conf`
- Cirrus CI enables flag before PCP installation

**For Discussion:**
- Should the PMDA periodically re-check this flag and log warnings?
- Should we provide a helper script to enable stats and restart PMCD?
- Is the current "check once at startup" approach sufficient?

### 2. Deprecated VM Statistics Fields

**Issue:** The `vm_statistics64.hits` and `vm_statistics64.lookups` fields exist in the macOS kernel API but are not populated on modern macOS (always return 0).

**Verified on:** macOS 15.x (Sequoia)

**Decision Made:** Removed `mem.cache_hits` and `mem.cache_lookups` metrics entirely from the PMDA, with comments documenting why. Item numbers 17-18 in CLUSTER_VMSTAT are reserved for these deprecated metrics.

**For Discussion:**
- Is removing metrics the right approach, or should we keep them with deprecation warnings?
- Should we monitor future macOS releases to see if Apple re-enables these fields?
- PCP philosophy question: expose all kernel API fields (even if zero) vs. only expose useful metrics?

### 3. Derived Metrics in pmrep vs. PMDA

**Issue:** During development, we attempted to add a cache hit ratio derived metric (`100 * mem.cache_hits / mem.cache_lookups`) in pmrep configuration. This proved problematic due to:
- Counter type restrictions (can't divide raw counters, need `rate()`)
- Type/semantic matching requirements in ternary operators
- Ultimately removed because base metrics were deprecated anyway

**For Discussion:**
- When should derived metrics be implemented in PMDA fetch functions vs. pmrep configs?
- Are there guidelines for which approach to use?
- Should complex calculations always live in the PMDA for better error handling?

### 4. File Permissions for New Source Files

**Issue:** During VFS implementation, new source files (`vfs.c`, `vfs.h`) were created with permissions 600 instead of 644, causing build isolation failures.

**Lesson Learned:** All source files must have 644 permissions (rw-r--r--) for proper build system operation.

**For Discussion:**
- Should the build system validate file permissions?
- Should we document this requirement more prominently?
- Are there git hooks or CI checks we could add to catch this?

### 5. PMNS "Disconnected Subtree" Errors

**Issue:** When adding new top-level namespaces (e.g., `vfs`), the `src/pmdas/darwin/root` file must be updated to include the namespace in the `root{}` block. Forgetting this causes "Disconnected subtree" PMNS parsing errors during build.

**For Discussion:**
- Could the build system detect this automatically?
- Should there be a validation tool for PMNS completeness?
- Is the current manual process error-prone?

---
