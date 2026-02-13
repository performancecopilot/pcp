# Darwin PMDA Quick Build Tools

This directory provides fast iteration for Darwin PMDA development (~10 second compile vs 30 minute full build).

It "cheats" by generating a minimal GNUmakefile that links against pre-built libraries from a complete Makepkgs run.

## Prerequisites

1. Run full build once (generates headers/libs):
   ```bash
   cd <repo-root> && ./Makepkgs --verbose
   ```

2. Initialize the quick-build environment:
   ```bash
   ./setup-local-pcp.sh
   ```

## Quick Commands

| Command | Purpose |
|---------|---------|
| `make clean && make` | Compile PMDA (~5-10s) |
| `./build-quick.sh` | Compile + smoke test |
| `./dev-test.sh` | Compile + PMDA tests |

## Recommended Workflow

Use the centralized test runner instead of these individual scripts:
```bash
cd <repo-root>/build/mac/test && ./run-all-tests.sh
```

## Source Location

**Edit source in** `src/pmdas/darwin/` - this directory only compiles it.

## See Also

- [macOS Build & Test Hub](../../build/mac/CLAUDE.md)
