# Darwin_proc PMDA Unit Tests

Unit tests that validate the Darwin_proc PMDA without requiring system installation.
Uses dbpmda to test the DSO directly.

**Run:** `./run-unit-tests.sh`

**Test files:**
- `test-proc.txt` - Process metrics (nprocs, psinfo, I/O stats, memory, file descriptors, run queue)

## Quick Testing
For rapid development, use centralized test runner:
```bash
cd ../../../../build/mac/test
./run-all-tests.sh  # Build + all tests (20-30 seconds)
```
