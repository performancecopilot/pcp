# Local CI Implementation for PCP

This document describes the implementation of local CI build support for PCP, allowing developers to run CI builds locally on macOS before submitting pull requests.

## Overview

The implementation adds macOS support and native architecture selection to the existing `build/ci/ci-run.py` script, enabling:
- Running PCP CI tests locally on macOS ARM64 (Apple Silicon)
- Fast native ARM64 builds without emulation overhead
- Optional amd64 emulation for exact CI parity
- Git worktree support for development branches

## Implementation Status

### ✅ Completed Stages

#### Stage 1: macOS Compatibility
**Commit:** 4f9402d513 - "Stage 1: Add macOS compatibility and platform detection"

- Detect host OS and architecture using Python's `platform` module
- Fix FileNotFoundError when reading `/etc/os-release` on macOS
- Add platform flags for container architecture selection
- Inject `--platform linux/amd64` or `--platform linux/arm64` into podman commands as needed

**Key Changes:**
- Added `is_macos()` and `is_arm64()` helper functions
- Updated ContainerRunner to handle missing `/etc/os-release`
- Added `self.platform_flags` to inject podman `--platform` directives

**Known Limitations:**
- Rosetta emulation has limitations with systemd containers on some podman configurations
- Recommend using native ARM64 builds on macOS instead

#### Stage 2: Architecture Detection & Native Build Support
**Commits:**
- f71c12f936 - "Stage 2: Add --native-arch and --emulate flags"
- 1162e7ffcc - "Fix git worktree support in container setup"
- d6ed466f9b - "Simplify git worktree fix - create minimal .git dir"
- 4510336441 - "Fix git worktree handling - remove file before creating dir"
- f42aaa321e - "Use sudo for git worktree .git file removal"
- 2e50128917 - "Add Ubuntu 24.04 ARM64 package list for local testing"

**Features Implemented:**
- `--native-arch` flag: Use native host architecture (ARM64 on macOS)
- `--emulate` flag: Force amd64 emulation even on ARM64 hosts
- Smart defaults: Use native arch on macOS, amd64 on Linux
- Git worktree support: Automatically fix broken `.git` references in containers
- ARM64 package list: Created Ubuntu 24.04 aarch64 variant for local builds

**Key Changes:**
- Added command-line argument parsing for `--native-arch` and `--emulate`
- Dynamic architecture detection in `main()` function
- Updated `ContainerRunner.__init__()` to accept and use `use_native_arch` parameter
- Automatic git worktree `.git` file replacement with minimal directory
- Added Ubuntu+24.04+aarch64 to package lists

**Behavior:**
```bash
# Default behavior on macOS: Uses native ARM64 (fast!)
python3 build/ci/ci-run.py ubuntu2404-container setup

# Force amd64 emulation for CI parity
python3 build/ci/ci-run.py --emulate ubuntu2404-container setup

# Explicit native arch on any platform
python3 build/ci/ci-run.py --native-arch ubuntu2404-container setup
```

### ⏳ Planned: Stage 3 - Quick Mode
Not yet implemented due to scope and token constraints.

**Planned Features:**
- `--quick` flag for multi-platform testing
- Priority hierarchy: CLI args → env var → config file → defaults
- Hardcoded default platform set: ubuntu2404, fedora43, centos-stream10
- Helpful error messages when platforms aren't specified

## Performance Characteristics

Based on testing with Ubuntu 24.04:

### Native ARM64 (macOS M4 Pro)
- ✅ Container builds successfully
- ✅ Commands execute directly in container
- ✅ No emulation overhead
- ✅ Fast package installation and compilation

### amd64 Emulation (macOS M4 Pro with Rosetta)
- ⚠️ Container builds successfully
- ⚠️ Rosetta emulation can have issues with systemd-based containers
- ⚠️ Requires Rosetta enabled in podman machine
- ⚠️ Slower due to instruction translation

**Recommendation:** Use native ARM64 for local feedback, only use amd64 emulation when CI parity is critical

## Usage Guide

### Basic Workflow

1. **First time setup:**
   ```bash
   # Ensure podman machine is running
   podman machine start

   # Verify Rosetta is enabled (if you want amd64 emulation)
   podman machine ssh default "ls /proc/sys/fs/binfmt_misc/rosetta"
   ```

2. **Set up container (default: native ARM64 on macOS):**
   ```bash
   cd /path/to/pcp-local-ci
   python3 build/ci/ci-run.py ubuntu2404-container setup
   ```

3. **Run build task:**
   ```bash
   python3 build/ci/ci-run.py ubuntu2404-container task build
   ```

4. **Run QA sanity tests:**
   ```bash
   python3 build/ci/ci-run.py ubuntu2404-container task qa_sanity
   ```

5. **Clean up:**
   ```bash
   python3 build/ci/ci-run.py ubuntu2404-container destroy
   ```

### Available Commands

```bash
# Setup container
python3 build/ci/ci-run.py [--native-arch | --emulate] PLATFORM setup

# Run specific task
python3 build/ci/ci-run.py PLATFORM task TASK_NAME

# Available tasks: setup, build, install, init_qa, qa_sanity, qa, copy_build_artifacts, copy_test_artifacts

# Get a shell in the container
python3 build/ci/ci-run.py PLATFORM shell

# Execute arbitrary command
python3 build/ci/ci-run.py PLATFORM exec COMMAND

# Clean up
python3 build/ci/ci-run.py PLATFORM destroy
```

## Supported Platforms

### Verified Working (Native ARM64)
- ubuntu2404-container (Ubuntu 24.04 LTS) ✅

### Planned Testing
- fedora43-container (Fedora 43)
- centos-stream10-container (CentOS Stream 10)

### Emulation Support (amd64)
All platforms support amd64 emulation with `--emulate` flag (with Rosetta caveats)

## Troubleshooting

### Issue: `FileNotFoundError: /etc/os-release`
**Solution:** Upgrade to the latest version with Stage 1 macOS support

### Issue: `exec: Exec format error` in amd64 emulation
**Solution:** Restart podman machine to restore Rosetta
```bash
podman machine stop
podman machine start
```

### Issue: Container exits immediately
**Solution:** Check podman machine logs
```bash
podman machine ssh default "journalctl -xe"
```

### Issue: Build fails with "not a git repository"
**Solution:** Should be auto-fixed by Stage 2 git worktree support. If not, check that `.git/config` exists in container.

## Architecture Overview

### Python Script Flow
```
ci-run.py main()
  ├─ Parse arguments (--native-arch, --emulate, platform)
  ├─ Detect host OS and architecture
  ├─ Load platform definition YAML
  ├─ Determine architecture to use
  ├─ Create appropriate runner (ContainerRunner, VirtualMachineRunner, DirectRunner)
  └─ Execute commands in runner context

ContainerRunner Setup
  ├─ Build container image with appropriate architecture
  ├─ Run container with systemd
  ├─ Copy PCP sources
  ├─ Fix git worktree references if needed
  ├─ Install build dependencies
  └─ Prepare artifact directories
```

### Platform Definitions
Each platform has a YAML file defining:
- Container base image and setup
- Build tasks (build, install, qa, etc.)
- Architecture specifications for artifactory

Example: `build/ci/platforms/ubuntu2404-container.yml`

## Future Enhancements

1. **Stage 3: Quick Mode**
   - `--quick` flag for subset of platforms
   - Configuration file support (.pcp-ci-quick)
   - Environment variable support (PCP_CI_QUICK_PLATFORMS)

2. **Multi-Architecture Package Lists**
   - Add ARM64 variants for other distributions
   - Fedora 43 aarch64
   - CentOS Stream 10 aarch64

3. **Rosetta Reliability**
   - Automatic Rosetta health checks
   - Auto-restart handling
   - Better error messages

4. **CI/CD Integration**
   - Generate build artifacts
   - Upload logs
   - Integrate with GitHub Actions

5. **Performance Optimization**
   - Layer caching across runs
   - Incremental builds
   - Parallel platform testing

## References

- Podman Machine Documentation: https://docs.podman.io/en/latest/markdown/podman-machine.1.html
- Rosetta 2 Support in Podman: https://podman-desktop.io/docs/podman/rosetta
- PCP QA Infrastructure: qa/admin/README
