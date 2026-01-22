# Darwin PMDA Unit Tests

Unit tests that validate the Darwin PMDA without requiring system installation.
Uses dbpmda to test the DSO directly.

**Run:** `./run-unit-tests.sh`

**Test files (`.txt` format using dbpmda commands):**
- `test-basic.txt` - Basic system metrics
- `test-cpu.txt` - CPU metrics
- `test-disk.txt` - Disk I/O metrics
- `test-memory.txt` - Memory metrics
- `test-memory-compression.txt` - Memory compression
- `test-sockstat.txt` - Socket statistics
- `test-tcp.txt` - TCP protocol metrics
- `test-tcpconn.txt` - TCP connection states
- `test-udp.txt` - UDP protocol metrics
- `test-icmp.txt` - ICMP protocol metrics
- `test-vfs.txt` - Virtual file system metrics
- `test-vmstat64-upgrade.txt` - VM statistics upgrade validation

## Quick Testing
For rapid development, use centralized test runner:
```bash
cd ../../../../build/mac/test
./run-all-tests.sh  # Build + all tests (20-30 seconds)
```
