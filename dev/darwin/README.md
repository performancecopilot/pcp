# Darwin PMDA Dev tools

This directory contains some build helper functions targetted at the Darwin(macOS) PMDA developer.  It helps
iteration cycles by focusing on building/testing the `src/pmdas/darwin*` build, but 'cheats' a little by crafting
a customized GNUMakefile to make it happen.

## Prerequisites

1. Run `${PROJECT_ROOT}/Makepkgs` - this ensures you have a complete and valid build tree to work off (but Darwin PMDA development still happens in src/pmdas/darwin*)
2. Initialize, one-time, the cheat-code GNUMakefile/builddefs:
```
./setup-local-pcp.sh
```

## Quick Builds

### Compile and run basic Darwin PMDA smoke tests
```
./build-quick.sh
```

### Build and Run simple PMDA tests
```
./dev-test.sh
```


# See Also
* [macOS-specific Build stuff](../../build/mac/README.md)