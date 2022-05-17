# GitHub Actions CI

## Workflows
Workflow descriptions are located in `.github/workflows`, platform specific PCP build tasks are stored in `build/ci/platforms`.

* CI: Triggered on push, runs the sanity QA group on assorted platforms
* QA: Runs the entire testsuite on pull requests and daily at 17:00 UTC, and publishes the results at https://performancecopilot.github.io/qa-reports/
* Release: Triggered when a new tag is pushed, creates a new release and pushes it to https://performancecopilot.jfrog.io/

## Reproducing test failures
```
build/ci/ci-run.py ubuntu2004-container|fedora34-container|centos8-container|... reproduce
```

## Debugging
Include the following action in a workflow, and connect via SSH:

```
- name: Setup tmate session
  uses: mxschmitt/action-tmate@v2
```

## Notes
Ubuntu 16.04 runs in a container even though there is a native Ubuntu 16.04 VM on GitHub actions, because:
* the `ci-run.py` script requires Python >= 3.6, but
* Ubuntu 16.04 contains Python 3.5 in the official repositories, and
* if a more recent version is installed using the `actions/setup-python` action, the included setuptools doesn't include the `--install-layout` option (this option is included only in the official Debian Python builds),
* however, this option is required by the PCP Python build.

# Artifactory
## Recalculate and sign package metadata
The repository metadata needs to be signed again after each change to the repository.
Artifactory cannot sign it automatically, because it does not have access to the GPG passphrase.

Run the following command to schedule recalculation and signing of package metadata:
`ARTIFACTORY_USER="$ARTIFACTORY_USER" ARTIFACTORY_TOKEN="$ARTIFACTORY_TOKEN" ARTIFACTORY_GPG_PASSPHRASE="$ARTIFACTORY_GPG_PASSPHRASE" ./build/ci/artifactory.py recalculate_metadata pcp-rpm-release pcp-deb-release`

## Deleting old versions to save storage space
The "Delete Versions" feature of the Artifatory UI doesn't work reliable, therefore we need to delete the versions by using the `jf` CLI [1]:
```
jf rt delete "pcp-rpm-release/*-5.3.0-1.*"
jf rt delete "pcp-deb-release/*_5.3.0-1_*"
```

[1] https://jfrog.com/getcli/
