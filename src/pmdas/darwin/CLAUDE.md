This directory contains the macOS(Darwin) PMDA source code, which provides native integration
for the macOS operating system.

## CRITICAL: macOS Development Constraints

### PCP Is NOT Installed Locally
**NEVER assume PCP tools (`pminfo`, `pmval`, `pmprobe`) are available on the development host.**

To check available metrics or PMNS structure:
- Read `src/pmdas/darwin/pmns` directly - this is the source of truth
- Do NOT try to run PCP commands locally

### Testing Environments

| Test Type | Where to Run | PCP Required? |
|-----------|--------------|---------------|
| **Unit tests** | Local (`./run-unit-tests.sh`) | No |
| **Integration tests** | Tart VM only (`/macos-qa-test`) | Yes (VM has it) |

### Git Commit Requirement

**ALL source code changes MUST be committed to git BEFORE running integration tests.**

The Tart VM clones the git repository - uncommitted local changes are invisible to it.

```bash
# Correct workflow:
git add <changed-files>
git commit -m "description"
/macos-qa-test              # Now VM can see your changes
```

## Code Style

* Keep method lengths short, with the "Single Responsibility principal" in mind
* Make the method names descriptive and readable
* Keep the code-style inline with other code in this directory
* Code can be reviewed by the `pcp-code-reviewer` sub-agent

## Adding New Metrics

### Pattern: Follow VFS Module Template
When adding new metric clusters, use `vfs.h`/`vfs.c` as the reference pattern:
- Create `<name>.h` with typedef struct and refresh/fetch prototypes
- Create `<name>.c` with `refresh_<name>()` using `sysctlbyname()` calls
- Add `CLUSTER_<NAME>` to `darwin.h` enum
- Wire into `pmda.c`: include header, add global vars, add to `darwin_refresh()` and `darwin_fetchCallBack()`
- Add metrics to `metrics.c`: include header, extern declaration, metric entries
- Add to `GNUmakefile` CFILES and HFILES

### CRITICAL: PMNS Root Namespace
When adding new top-level metric namespaces:
1. Define metrics in `pmns` file (e.g., `ipc { ... }`)
2. **MUST** add namespace to `root` file's root block (e.g., add `ipc` to root list)
3. Failure to update `root` causes "Disconnected subtree" PMNS parsing errors during build

**Example:**
```diff
root {
    kernel
    ...
+   ipc
}
```

## Testing & QA

### Unit Tests (Run Locally)
Unit tests do NOT require PCP installation:
```bash
cd src/pmdas/darwin/test && ./run-unit-tests.sh
```

### Integration Tests (Tart VM Only)
Integration tests MUST run in the isolated Tart VM environment. Use the `macos-darwin-pmda-qa` agent or invoke `/macos-qa-test`:

**Before running:**
1. Commit all source changes to git
2. Then run: `/macos-qa-test`

The agent handles `cirrus run --dirty` and reports results.

### Code Review
Use the `pcp-code-reviewer` agent to review changes against PCP project standards


