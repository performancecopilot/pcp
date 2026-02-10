# GitHub Actions CI

## Workflows
Workflow descriptions are located in `.github/workflows`, platform specific PCP build tasks are stored in `build/ci/platforms`.

* CI: Triggered on push, runs the sanity QA group on assorted platforms
* QA: Runs the entire testsuite on pull requests and daily at 17:00 UTC, and publishes the results at https://performancecopilot.github.io/qa-reports/
* Release: Triggered when a new tag is pushed, creates a new release and pushes it to https://packagecloud.io/performancecopilot/pcp

## Quick Start - Running CI Locally

Test your changes against a single platform:
```bash
# Full CI pipeline (setup → build → install → init_qa → qa_sanity)
build/ci/ci-run.py ubuntu2404-container reproduce --until qa_sanity

# Just setup and build (quick feedback on compilation issues)
build/ci/ci-run.py ubuntu2404-container reproduce --until build

# Run individual tasks
build/ci/ci-run.py ubuntu2404-container task build
build/ci/ci-run.py ubuntu2404-container task qa_sanity
```

## Supported Platforms for Local Testing

See `.github/workflows/ci.yml` for the authoritative list of platforms being tested in CI. Currently tested platforms include:

- ubuntu1804-i386-container (Ubuntu 18.04, 32-bit)
- ubuntu2004-container (Ubuntu 20.04)
- ubuntu2204-container (Ubuntu 22.04)
- ubuntu2404-container (Ubuntu 24.04 LTS, recommended for new development)
- fedora42-container (Fedora 42)
- fedora43-container (Fedora 43)
- centos-stream8-container (CentOS Stream 8)
- centos-stream9-container (CentOS Stream 9)
- centos-stream10-container (CentOS Stream 10)

Additional platforms available in the codebase but not in the default CI matrix (see comments in `.github/workflows/ci.yml`):
- debian12-container
- debian13-container
- fedora-rawhide-container

## Architecture Support

By default, the CI scripts use your native host architecture:
- **macOS ARM64 (Apple Silicon)**: Runs native ARM64 containers (fast, no emulation)
- **macOS Intel**: Runs amd64 containers natively
- **Linux**: Always uses amd64 containers

To force amd64 emulation on macOS for exact CI parity (requires Rosetta):
```bash
build/ci/ci-run.py --emulate ubuntu2404-container reproduce
```

## Troubleshooting (macOS)

If you see "exec: Exec format error" with amd64 emulation, Rosetta may have become unstable. Restart the podman machine:
```bash
podman machine stop && podman machine start
```

## Reproducing test failures
```
build/ci/ci-run.py ubuntu2004-container|fedora38-container|centos9-container|... reproduce
```

## Debugging
Include the following action in a workflow, and connect via SSH:

```
- name: Setup tmate session
  uses: mxschmitt/action-tmate@v2
```

# Packagecloud

## Deleting old versions to save storage space
We have limited space using the free packagecloud account option [1], so may need to respond to requests to free up used space from old builds.  This is done through the web interace.

[1] https://packagecloud.io/performancecopilot/pcp
