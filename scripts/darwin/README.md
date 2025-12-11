# Darwin PMDA Development and Testing

This directory contains tools for rapid Darwin PMDA development and testing on macOS.

## Directory Structure

```
scripts/darwin/
├── README.md                    # This file
├── dev/
│   ├── Makefile                # Standalone build for quick iteration
│   └── build-quick.sh          # Quick build script
├── test/
│   ├── unit/                   # Unit tests (pre-install, uses dbpmda)
│   │   ├── run-unit-tests.sh
│   │   ├── test-basic.txt
│   │   ├── test-memory.txt
│   │   ├── test-cpu.txt
│   │   └── test-disk.txt
│   ├── integration/            # Integration tests (post-install)
│   │   ├── run-integration-tests.sh
│   │   └── test-pmstat.sh
│   └── fixtures/               # Test data and expected outputs
└── ci/
    └── test-in-ci.sh           # GitHub Actions test orchestrator
```

## Prerequisites

**One-time setup**: PCP must be fully built and installed before using the quick build:
```bash
cd <repo-root>
./Makepkgs --verbose
# This configures, builds, and installs PCP
# Takes ~5-30 minutes, only needs to be done once
```

After the full build completes, you can use either workflow below.

## Quick Development Workflow

### Option 1: Full PCP Build (standard, if you need other changes)
```bash
# From repo root
./Makepkgs --verbose
```

### Option 2: Quick Darwin PMDA Only Build (for iteration) ⚡
```bash
# After Makepkgs has completed (see Prerequisites), use this for rapid iteration
cd scripts/darwin/dev
make clean && make      # 5-10 seconds instead of 5-30 minutes!
```

**Note:** The quick build only works after the prerequisite full build has completed at least once. After that, Darwin PMDA can be rebuilt in seconds without rebuilding all of PCP.

## Testing

### Unit Tests (Pre-Installation)
Tests the Darwin PMDA in isolation using `dbpmda`:

```bash
cd scripts/darwin/test/unit
./run-unit-tests.sh
```

**What it tests:**
- PMDA loads correctly
- All metrics are accessible
- Metric metadata is correct
- Basic value sanity checks

**Requirements:** Built Darwin PMDA (doesn't need to be installed)

### Integration Tests (Post-Installation)
Tests the Darwin PMDA through real PCP tools:

```bash
cd scripts/darwin/test/integration
./run-integration-tests.sh
```

**What it tests:**
- `pminfo` can query Darwin metrics
- `pmval` can fetch metric values
- `pmstat` shows macOS-specific data
- Metric values are reasonable

**Requirements:** PCP installed and pmcd running

## CI Integration

The GitHub Actions workflow (`.github/workflows/macOS.yml`) runs both test phases:
1. **Unit tests** - After build, before installation
2. **Integration tests** - After installation

## Adding New Metrics

When adding new macOS metrics to the Darwin PMDA:

1. **Add metric to source**: Edit `src/pmdas/darwin/pmda.c` and related files
2. **Update PMNS**: Edit `src/pmdas/darwin/pmns`
3. **Quick build**: `cd scripts/darwin/dev && make`
4. **Unit test**: Add test case to `test/unit/test-<cluster>.txt`
5. **Integration test**: Add validation to `test/integration/test-pmstat.sh`
6. **Run tests**: `./test/unit/run-unit-tests.sh`
7. **Full build**: When ready, run full `./Makepkgs`

## Debugging

### Test a specific metric
```bash
# Using dbpmda
echo "open dso ../../src/pmdas/darwin/pmda_darwin.dylib darwin_init 78
fetch darwin.mem.physmem" | dbpmda -n ../../src/pmdas/darwin/pmns

# Using pminfo (requires installation)
pminfo -f darwin.mem.physmem
```

### Run Darwin PMDA in daemon mode
```bash
cd src/pmdas/darwin
./pmdadarwin -d 78 -l darwin.log
```

### Check pmcd logs
```bash
tail -f /var/log/pcp/pmcd/pmcd.log
```
