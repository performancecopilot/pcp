# Darwin PMDA Development & Testing

Fast iteration tools for Darwin PMDA development on macOS.

## ⚠️ macOS Development Constraints

1. **PCP is NOT installed locally** - Don't run `pminfo`/`pmval`/`pmprobe`. Read `src/pmdas/darwin/pmns` instead.
2. **Commit before VM tests** - Tart VM only sees committed git changes.
3. **Unit tests = local, Integration tests = VM only**

### Git Commit Requirement

**ALL changes MUST be committed to git BEFORE running Tart VM tests.**

This includes:
- Source code changes (`src/pmdas/darwin*`)
- Build/test scripts in THIS directory (`build/mac/`)
- Installation scripts, test fixtures, etc.

The Tart VM clones the git repository - uncommitted local changes are invisible to it.

---

## Prerequisites (One-Time Setup)

PCP must be fully built before using quick builds:

```bash
cd <repo-root>
./Makepkgs --verbose
```

This takes 5-30 minutes and only needs to be done once. It generates headers, builds libraries, and installs PCP.

**Verify setup:**
```bash
cd dev/darwin
make check-deps
```

## Quick Development Workflow

### Build + Test (Recommended)
```bash
cd build/mac/test
./run-all-tests.sh
```

Builds PMDA (~5-10s) and runs unit tests (~10-20s). Runs integration tests if pmcd is available.

### Build Only
```bash
cd dev/darwin
make clean && make
```

### Test Only

**Unit tests** (no installation needed):
```bash
# Darwin PMDA
cd src/pmdas/darwin/test
./run-unit-tests.sh

# Darwin_proc PMDA
cd src/pmdas/darwin_proc/test
./run-unit-tests.sh

# Both PMDAs
cd build/mac/test
./run-unit-tests.sh
```

**Integration tests** (Tart VM only - do NOT run locally):

Integration tests require PCP installed and pmcd running. The Tart VM provides this environment.

**Before running:**
1. Commit all changes to git (source code AND build/mac scripts)
2. Use `/macos-qa-test` skill or `macos-darwin-pmda-qa` agent

The VM clones the git repo, so uncommitted changes are invisible

### Centralized Test Orchestration
Quick test runners available in `build/mac/test/`:
- `./run-all-tests.sh` - Build + all unit tests + integration tests (20-30 seconds)
- `./run-unit-tests.sh` - Unit tests only for both PMDAs (no build)
- `./run-integration-tests.sh` - Integration tests only (no build)

## Adding New Metrics

1. Edit source: `src/pmdas/darwin/pmda.c`
2. Update PMNS: `src/pmdas/darwin/pmns`
3. Quick build: `cd dev/darwin && make`
4. Add test: `src/pmdas/darwin/test/test-<cluster>.txt`
5. Run tests: `cd build/mac/test && ./run-all-tests.sh`

## Detailed Documentation

For in-depth procedures and troubleshooting:
- `MACOS_DEVELOPMENT.md` - Tart VM setup for clean-room builds
- `qa/TESTING.md` - pmcd launchctl test plan

