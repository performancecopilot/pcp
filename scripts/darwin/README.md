# Darwin PMDA Development & Testing

Fast iteration tools for Darwin PMDA development on macOS.

## Prerequisites (One-Time Setup)

PCP must be fully built before using quick builds:

```bash
cd <repo-root>
./Makepkgs --verbose
```

This takes 5-30 minutes and only needs to be done once. It generates headers, builds libraries, and installs PCP.

**Verify setup:**
```bash
cd scripts/darwin/dev
make check-deps
```

## Quick Development Workflow

### Build + Test (Recommended)
```bash
cd scripts/darwin/test
./quick-test.sh
```

Builds PMDA (~5-10s) and runs unit tests (~10-20s). Runs integration tests if pmcd is available.

### Build Only
```bash
cd scripts/darwin/dev
make clean && make
```

### Test Only

**Unit tests** (no installation needed):
```bash
cd scripts/darwin/test/unit
./run-unit-tests.sh
```

**Integration tests** (requires PCP installed and pmcd running):
```bash
cd scripts/darwin/test/integration
./run-integration-tests.sh
```

## Adding New Metrics

1. Edit source: `src/pmdas/darwin/pmda.c`
2. Update PMNS: `src/pmdas/darwin/pmns`
3. Quick build: `cd scripts/darwin/dev && make`
4. Add test: `test/unit/test-<cluster>.txt`
5. Run tests: `cd scripts/darwin/test && ./quick-test.sh`

