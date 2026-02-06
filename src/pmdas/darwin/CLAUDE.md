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
* Code **MUST** be reviewed and approved by the `pcp-code-reviewer` sub-agent

## Adding New Metrics

### Pattern: Follow VFS Module Template
When adding new metric clusters, use `vfs.h`/`vfs.c` as the reference pattern:
- Create `<name>.h` with typedef struct and refresh/fetch prototypes
- Create `<name>.c` with `refresh_<name>()` using `sysctlbyname()` calls
- Add `CLUSTER_<NAME>` to `darwin.h` enum
- Wire into `pmda.c`: include header, add global vars, add to `darwin_refresh()` and `darwin_fetchCallBack()`
- Add metrics to `metrics.c`: include header, extern declaration, metric entries
- Add to `GNUmakefile` CFILES and HFILES

### CRITICAL: Instance Domain Registration
When adding new instance domains (standard PMDA requirement):
1. Add enum entry to `darwin.h` (e.g., `FAN_INDOM`)
2. **MUST** add corresponding entry to `indomtab[]` array in `pmda.c`
3. Dynamic instance domains: `{ INDOM_NAME, 0, NULL }` (populated at runtime)
4. Missing indomtab entry causes: "Undefined instance domain serial (N)" - entire PMDA fails to load

### Instance Domain Update Pattern
Standard PMDA pattern (see `disk.c:update_disk_indom()` or Linux PMDA `proc_buddyinfo.c`):
```c
indom->it_set = realloc(indom->it_set, count * sizeof(pmdaInstid));
indom->it_numinst = count;
```
No helper function exists - directly manipulate `it_set` and `it_numinst`.

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

## SMC (System Management Controller) Access

The Darwin PMDA includes thermal monitoring via Apple's SMC (System Management Controller),
which provides access to temperature sensors and fan metrics.

### Important: Reverse-Engineered APIs

SMC access is **community reverse-engineered** and **NOT officially supported by Apple**.
The APIs may change between macOS versions and may require entitlements on newer systems.

### Reference Documentation

- **iSMC** (https://github.com/dkorunic/iSMC) - CLI tool with Apple Silicon support
  - Reference for SMC key patterns on M1/M2/M3 Macs
  - Demonstrates sp78 and fpe2 format conversions

- **SMCKit** (https://github.com/beltex/SMCKit) - Comprehensive Swift library
  - Extensive SMC key documentation for Intel and Apple Silicon
  - Reference for fan control and thermal sensor keys

### SMC Key Formats

| Format | Type | Conversion | Usage |
|--------|------|------------|-------|
| sp78 | Signed fixed-point | ÷256 | Temperature (°C) |
| fpe2 | Unsigned fixed-point | ÷4 | Fan RPM |
| ui8 | Unsigned 8-bit | Direct | Fan count, flags |

### Key Patterns by Platform

**Apple Silicon (M1/M2/M3):**
- Temperature: `Tp01` (CPU die), `Tg01` (GPU die), `TCXC` (package)
- Fan: `FNum` (count), `F0Ac`/`F1Ac` (speed), `F0Tg`/`F1Tg` (target)

**Intel Macs:**
- Temperature: `TC0P` (CPU proximity), `TG0D` (GPU die)
- Fan patterns same as Apple Silicon

### Graceful Degradation

The thermal subsystem degrades gracefully when SMC access fails:
- Thermal pressure metrics always work (notify API, no SMC required)
- Temperature/fan metrics return `PM_ERR_APPVERSION` if SMC unavailable
- Fanless Macs (MacBook Air) report `hinv.nfan=0` correctly

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


