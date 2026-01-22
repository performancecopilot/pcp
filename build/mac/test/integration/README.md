# Darwin PMDA Integration Tests

Integration tests that validate both darwin and darwin_proc PMDAs through real PCP tools.
Requires PCP installed and pmcd running.

**Run:** `./run-integration-tests.sh`

**Test scripts:**
- `run-integration-tests.sh` - Main test runner (17 test groups for both PMDAs)
- `test-pmstat.sh` - pmstat tool validation
- `test-pmrep-macstat.sh` - pmrep :macstat config validation

**Test coverage:**
- Darwin PMDA: Basic metrics, memory, CPU, disk, network protocols, VFS
- Darwin_proc PMDA: Process metrics, I/O statistics, file descriptors
- Tool integration: pmstat, pmrep, pminfo, pmval

## Quick Testing
```bash
cd ..
./run-all-tests.sh  # Build + unit + integration (20-30 seconds)
```
