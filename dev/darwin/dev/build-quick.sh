#!/bin/bash
#
# Quick build script for Darwin PMDA development
# Builds just the Darwin PMDA without full PCP build
#
# REQUIREMENT: PCP must be configured first (one-time setup)
#

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR"

echo "========================================"
echo "Quick Darwin PMDA Build"
echo "========================================"
echo

# Check if PCP has been fully built
echo "Checking if PCP is fully built..."
if ! make check-configured > /dev/null 2>&1; then
    echo
    echo "ERROR: PCP must be fully built and installed first"
    echo
    echo "Run this command from the repo root:"
    echo "  ./Makepkgs --verbose"
    echo
    echo "This takes ~5-30 minutes but only needs to be done once."
    echo
    exit 1
fi
echo "✓ PCP is built and installed"
echo

# Check dependencies
echo "Checking build dependencies..."
make check-deps
echo

# Clean previous build
echo "Cleaning previous build..."
make clean
echo

# Build
echo "Building Darwin PMDA..."
time make all
echo

# Run smoke test
echo "Running smoke test..."
make test
echo

echo "========================================"
echo "Build complete!"
echo "========================================"
echo "Binary: $(pwd)/pmdadarwin"
echo "DSO:    $(pwd)/pmda_darwin.dylib"
echo
echo "Next steps:"
echo "  1. Run darwin tests:     cd ../../../src/pmdas/darwin/test && ./run-unit-tests.sh"
echo "  2. Run darwin_proc tests: cd ../../../src/pmdas/darwin_proc/test && ./run-unit-tests.sh"
echo "  3. Run all tests:        cd ../../../build/mac/test && ./run-all-tests.sh"
echo "  4. Install to system:    make install"
echo "  3. Test with pminfo:     pminfo -f mem.physmem"
echo
