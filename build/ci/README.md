# GitHub Actions CI

## Workflows
Workflow descriptions are located in `.github/workflows`, platform specific PCP build tasks are stored in `./build/ci/platforms`.

* CI: Triggered on push and on pull requests, runs the sanity QA group on assorted platforms
* Daily CI: Runs a full QA run daily at 19:00 UTC and publishes the results at https://pcp.io/qa-reports/
* Release: Triggered when a new tag is pushed, creates a new release and pushes it to https://performancecopilot.jfrog.io/

## Reproducing test failures
```
./build/ci/run.py --runner container --platform ubuntu2004|fedora32|centos8|... reproduce
```

## Debugging
Include the following action in a workflow, and connect via SSH:

```
- name: Setup tmate session
  uses: mxschmitt/action-tmate@v2
```

## Notes
Ubuntu 16.04 runs in a container even though there is a native Ubuntu 16.04 VM on GitHub actions, because:
* the `run.py` script requires Python >= 3.6, but
* Ubuntu 16.04 contains Python 3.5 in the official repositories, and
* if a more recent version is installed using the `actions/setup-python` action, the included setuptools doesn't include the `--install-layout` option (this option is included only in the official Debian Python builds),
* however, this option is required by the PCP Python build.
