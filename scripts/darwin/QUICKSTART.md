# Darwin PMDA Quick Start Guide

## For Rapid Development

### Initial Setup (One-time - ~5-30 minutes)

The quick build requires that PCP has been fully built and installed at least once:

```bash
cd <repo-root>
./Makepkgs --verbose
```

This does everything needed:
- Runs configure to generate platform headers
- Compiles all of PCP
- Installs libraries and headers
- Creates the PMDA

After this completes, you'll have fast iteration builds available.

**Verify setup is complete:**
```bash
cd scripts/darwin/dev
make check-deps   # Should show all ✓ checks passed
```

### Fast Development Cycle

#### Option 1: Build + Test in One Command
```bash
cd scripts/darwin/test
./quick-test.sh
```
This will:
1. Build Darwin PMDA (~5-10 seconds)
2. Run unit tests (~10-20 seconds)
3. Run integration tests if pmcd is available

#### Option 2: Manual Steps

**Build only:**
```bash
cd scripts/darwin/dev
make clean && make
```

**Unit tests (no installation needed):**
```bash
cd scripts/darwin/test/unit
./run-unit-tests.sh
```

**Integration tests (requires PCP installed):**
```bash
cd scripts/darwin/test/integration
./run-integration-tests.sh
```

## Development Workflow Examples

### Adding a New Metric

1. **Edit source code:**
```bash
vim src/pmdas/darwin/pmda.c
# Add your new metric to metrictab[]
```

2. **Update PMNS:**
```bash
vim src/pmdas/darwin/pmns
# Add metric name and PMID
```

3. **Quick build:**
```bash
cd scripts/darwin/dev
make
```
⏱️ ~5-10 seconds

4. **Test with dbpmda:**
```bash
cd scripts/darwin/test/unit
# Add test case to test-<cluster>.txt
./run-unit-tests.sh
```
⏱️ ~10 seconds

5. **Install and test:**
```bash
cd scripts/darwin/dev
sudo make install  # Installs to system PCP
pminfo -f darwin.your.new.metric
```

### Debugging a Metric

**Interactive testing with dbpmda:**
```bash
# From repo root
cd scripts/darwin/dev
make  # Ensure built

# Interactive mode
dbpmda -n ../../../src/pmdas/darwin/pmns
> open dso ./pmda_darwin.dylib darwin_init 78
> fetch darwin.mem.physmem
> desc darwin.mem.physmem
> quit
```

**Test with real pmcd:**
```bash
# Install latest version
cd scripts/darwin/dev
sudo make install

# Watch metric values
pmval -t 1 darwin.mem.physmem

# Check metric metadata
pminfo -dT darwin.mem.physmem
```

## CI/CD Integration

The GitHub Actions workflow (`.github/workflows/macOS.yml`) automatically runs:

1. **After Build**: Unit tests via `scripts/darwin/test/unit/run-unit-tests.sh`
2. **After Install**: Integration tests via `scripts/darwin/test/integration/run-integration-tests.sh`

## Troubleshooting

### "fatal error: 'platform_defs.h' file not found" or "libpcp not found"

These errors mean PCP hasn't been fully built and installed yet. You need to complete the Initial Setup:

```bash
cd <repo-root>
./Makepkgs --verbose
```

This is a one-time setup that takes ~5-30 minutes. After it completes, the quick build will work.

### "libpcp not found" after full build
```bash
# Check PCP installation
brew list pcp
# or
ls -la /usr/local/lib/libpcp*

# Set library path if needed
export DYLD_LIBRARY_PATH=/usr/local/lib:$DYLD_LIBRARY_PATH
```

### "dbpmda not found"

This means PCP tools aren't installed. Complete the Initial Setup (./Makepkgs) from the repo root - it will install dbpmda.

### "Unit tests fail"
```bash
# Check build succeeded
ls -la scripts/darwin/dev/pmda_darwin.dylib

# Check PMNS file exists
ls -la src/pmdas/darwin/pmns

# Try manual dbpmda test
cd scripts/darwin/dev
echo "open dso ./pmda_darwin.dylib darwin_init 78
fetch hinv.ncpu
quit" | dbpmda -n ../../../src/pmdas/darwin/pmns
```

### "Integration tests fail"
```bash
# Check pmcd is running
pgrep pmcd

# Start pmcd if needed
sudo pmcd start

# Check Darwin PMDA is loaded
pminfo -f pmcd.agent.status | grep darwin

# Check pmcd logs
sudo tail -f /var/log/pcp/pmcd/pmcd.log
```

## Performance Expectations

- **Full PCP build**: 5-30 minutes
- **Darwin PMDA quick build**: 5-10 seconds
- **Unit tests**: 10-20 seconds
- **Integration tests**: 20-30 seconds
- **Total edit-test cycle**: < 1 minute

## Next Steps

- **Add tests**: Create new test files in `test/unit/` for new metrics
- **Extend metrics**: See `src/pmdas/darwin/pmda.c` for metric implementation
- **Review logs**: Check `scripts/darwin/dev/*.log` for debug output
- **CI logs**: Check GitHub Actions for automated test results
