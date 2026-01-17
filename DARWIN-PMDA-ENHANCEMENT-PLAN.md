# Darwin PMDA Enhancement Plan

## Current Status

**Last Updated:** 2026-01-17
**Current Phase:** Phase 3B - Addressing PR #2442 review feedback
**Current Step:** 3B.5 (TCP state validation logging)
**Pull Request:** https://github.com/performancecopilot/pcp/pull/2442

## Progress Tracker

| Phase | Step | Description | Status | Commit |
|-------|------|-------------|--------|--------|
| **1** | **Memory Enhancement** | | | |
| 1 | 1.1a | vm_statistics64 API upgrade | ✅ COMPLETED | b49d238c85 |
| 1 | 1.1b | Memory compression metrics | ✅ COMPLETED | 9db0865001 |
| 1 | 1.2 | VFS statistics | ✅ COMPLETED | 71f12b992a |
| **2** | **Network Enhancement** | | | |
| 2 | 2.1 | UDP protocol statistics | ✅ COMPLETED | daf0f8c892 |
| 2 | 2.2 | ICMP protocol statistics | ✅ COMPLETED | 14654a6e2a |
| 2 | 2.3 | Socket counts | ✅ COMPLETED | 50ab438ac3 |
| 2 | 2.4 | TCP connection states | ✅ COMPLETED | 96a4191fcd |
| 2 | 2.5-pre | Enable TCP stats in Cirrus CI | ✅ COMPLETED | 1d967ef777 |
| 2 | 2.5a | TCP protocol statistics | ✅ COMPLETED | 2bf73ecef5, 540e5304b2 |
| 2 | 2.5b | TCP access control detection/docs | ✅ COMPLETED | f5c406e52a |
| 2 | 2.5c | TCP auto-enable config | ⏸️ DEFERRED | (for maintainer discussion) |
| 2 | 2.6 | pmrep macOS monitoring views | ✅ COMPLETED | 9afb1b9e9d, 3a2e05e5da |
| **3** | **Process Enhancement** | | | |
| 3 | 3.1 | Process I/O statistics | ✅ COMPLETED | e0b925a347 |
| 3 | 3.2 | Process file descriptor count | ✅ COMPLETED | (ready for commit) |
| **3B** | **PR #2442 Review Feedback** | | | |
| 3B | 3B.1 | Copyright header updates | ✅ COMPLETED | d9678a39a0 |
| 3B | 3B.2 | Test infrastructure relocation | ✅ COMPLETED | 1f521b1ad1 |
| 3B | 3B.3 | TCP granular error metrics | ✅ COMPLETED | 459ab13d36 |
| 3B | 3B.4 | UDP granular error metrics | ✅ COMPLETED | 3e19fa6a45 |
| 3B | 3B.5 | TCP state validation logging | ⏭️ NEXT | |
| **4** | **Finalization** | | | |
| 4 | 4.1 | Transform plan → documentation | 📋 PENDING | |
| 4 | 4.2 | Refactor pmda.c legacy code | 📋 PENDING | |

---

## Summary of Completed Work

### Phase 1: Memory Subsystem
- Upgraded to 64-bit memory statistics API (vm_statistics64)
- Added memory compression metrics (compressed pages, compressions, decompressions)
- Added VFS resource metrics (files, vnodes, processes, threads)

### Phase 2: Network Subsystem
- Implemented UDP protocol statistics (5 metrics)
- Implemented ICMP protocol statistics (8 metrics)
- Added socket count metrics (TCP/UDP)
- Added TCP connection state tracking (11 states)
- Implemented TCP protocol statistics (15 metrics) with access control handling
- Created 6 pmrep monitoring views (:macstat, :macstat-x, :macstat-mem, :macstat-dsk, :macstat-tcp, :macstat-proto)

### Phase 3: Process Subsystem
- Added per-process I/O statistics (read_bytes, write_bytes)
- Added per-process file descriptor count

### Phase 3B: PR Review Feedback
- Updated copyright headers for all new files
- Relocated test infrastructure to top-level dev/ directory
- Added granular TCP error metrics (4 individual + 1 aggregate)
- Added granular UDP error metrics (3 individual + 1 aggregate)

**Total Metrics Added:** 60+ new metrics across memory, network, and process subsystems

---

## Execution Workflow

1. **Execute one step at a time** - complete implementation, tests, docs
2. **Run test runner** after each step
3. **Run code review** - use `pcp-code-reviewer` agent
4. **Fix any issues** - address findings from code review
5. **Commit only after approval** - then proceed to next step

---

## Critical Files

| File | Purpose |
|------|---------|
| `src/pmdas/darwin/pmda.c` | Main PMDA coordination and dispatch |
| `src/pmdas/darwin/metrics.c` | Metric table (metrictab[]) |
| `src/pmdas/darwin/<subsystem>.c` | Data collection (refresh_*() functions) |
| `src/pmdas/darwin/<subsystem>.h` | Structure definitions |
| `src/pmdas/darwin/pmns` | Metric namespace hierarchy |
| `src/pmdas/darwin/help` | Metric documentation |
| `dev/darwin/test/unit/test-*.txt` | Unit tests (dbpmda) |
| `dev/darwin/test/integration/run-integration-tests.sh` | Integration tests |

---

## Code Organization Pattern

New subsystems follow a modular pattern:
- **subsystem.h**: Structure definitions and function declarations
- **subsystem.c**: `refresh_subsystem()` and `fetch_subsystem()` implementations
- **pmda.c**: Cluster definition, metrictab entries, dispatch wiring
- **GNUmakefile**: Build integration

---

## Testing Requirements

### Before Running Tests
**CRITICAL:** Commit all changes to git first. The test agent uses `git archive` and only sees committed code.

### Test Types
1. **Unit Tests**: `dev/darwin/test/unit/test-*.txt` (dbpmda command files)
2. **Integration Tests**: `dev/darwin/test/integration/run-integration-tests.sh`
3. **Test Runner**: Use `macos-darwin-pmda-qa` agent (Cirrus VM environment)

### Test Pattern
```
# Unit test format (test-<feature>.txt)
desc <metric>
fetch <metric>

# Integration test format (run-integration-tests.sh)
run_test "<description>" "pminfo -f <metric>"
run_test "<description>" "validate_metric <metric> <validation>"
```

---

## Code Quality Standards

### Error Handling
Use **fail-fast** pattern (return immediately on error):
```c
int refresh_example(example_t *data) {
    size_t size = sizeof(data->field);
    if (sysctlbyname("kern.example", &data->field, &size, NULL, 0) == -1)
        return -oserror();  // Fail fast
    return 0;
}
```

### Code Style
- **Indentation**: Use TABS, not spaces
- **Function names**: Descriptive and readable
- **Method length**: Keep short, single responsibility
- **Comments**: Explain why, not what

### Code Review
Always run `pcp-code-reviewer` agent before committing. Fix all issues including "minor" suggestions.

---

## Remaining Work

### Step 3B.5: TCP State Validation Logging
Add diagnostic logging for out-of-range TCP connection states with one-trip guard to prevent log flooding.

### Step 4.1: Documentation
Transform this plan into permanent development documentation.

### Step 4.2: Code Refactoring
Extract legacy fetch functions from pmda.c into dedicated subsystem files for better maintainability.

---

## Key Technical Notes

1. **sysctl() is a syscall** - not fork/exec, safe and efficient
2. **TCP stats access controlled** - `net.inet.tcp.disable_access_to_stats` flag (default: disabled)
3. **UDP/ICMP stats work** - via direct sysctl struct access
4. **Each step independently committable** - with full test coverage
5. **Tab indentation** - darwin PMDA uses tabs, not spaces

---

## Reference Links

- **PR #2442**: https://github.com/performancecopilot/pcp/pull/2442
- **Test Infrastructure**: `dev/darwin/`
- **PMDA Source**: `src/pmdas/darwin/`
- **pmrep Configs**: `src/pmrep/conf/macstat.conf`
