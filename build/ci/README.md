# GitHub Actions CI

## Workflows
Workflow descriptions are located in `.github/workflows`, platform specific PCP build tasks are stored in `build/ci/platforms`.

* CI: Triggered on push, runs the sanity QA group on assorted platforms
* QA: Runs the entire testsuite on pull requests and daily at 17:00 UTC, and publishes the results at https://performancecopilot.github.io/qa-reports/
* Release: Triggered when a new tag is pushed, creates a new release and pushes it to https://packagecloud.io/performancecopilot/pcp

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
