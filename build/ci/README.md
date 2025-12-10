# GitHub Actions CI

## Workflows
Workflow descriptions are located in `.github/workflows`, platform specific PCP build tasks are stored in `build/ci/platforms`.

* CI: Triggered on push, runs the sanity QA group on assorted platforms
* QA: Runs the entire testsuite on pull requests and daily at 17:00 UTC, and publishes the results at https://performancecopilot.github.io/qa-reports/
* Release: Triggered when a new tag is pushed, creates a new release and pushes it to https://packagecloud.io/performancecopilot/pcp

## Local CI on macOS

PCP CI can now be run locally on macOS systems, useful for faster feedback before submitting PRs.

### Architecture Support

By default, the CI scripts use your native host architecture:
- **macOS ARM64 (Apple Silicon)**: Runs native ARM64 containers (fast, no emulation)
- **macOS Intel**: Runs amd64 containers natively
- **Linux**: Always uses amd64 containers

To force amd64 emulation on macOS for exact CI parity (requires Rosetta):
```bash
build/ci/ci-run.py --emulate ubuntu2404-container reproduce
```

### Quick Start

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

### Supported Platforms for Local Testing
- ubuntu2404-container (Ubuntu 24.04 LTS, recommended)
- fedora43-container (Fedora 43)
- centos-stream10-container (CentOS Stream 10)

### Troubleshooting

If you see "exec: Exec format error" with amd64 emulation, Rosetta may have been lost. Restart the podman machine:
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
