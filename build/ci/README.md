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
- ubuntu2404-container (Ubuntu 24.04 LTS)
- ubuntu2604-container (Ubuntu 26.04 LTS, recommended for new development)
- fedora43-container (Fedora 43)
- fedora44-container (Fedora 44)
- fedora45-container (Fedora 45)
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

## Release artifact signing

The [Release workflow](.github/workflows/release.yml) signs every RPM and
DEB package with the PCP maintainers key after QA has completed and
before the packages are uploaded to Packagecloud.  Packagecloud does not
re-sign uploaded packages, so this is the only signature consumers see.

The signing key is `pcp-masters`, fingerprint
`D87805D281D85A13539CB4BFA51D3ADCDEA2C114`.  It is the key published at
https://pcp.io/download.html and can be fetched with
[`scripts/gpg-public-keys`](../../scripts/gpg-public-keys).

Signing runs in the pinned container from
[`signing.Dockerfile`](signing.Dockerfile) because debsigs 0.2.2 or later
is required for zstd-compressed Debian packages and for signature
verification.  The Runner image is not new enough for that.

### Required secrets

Set these on the upstream repository (Settings -> Secrets and variables ->
Actions), or on a fork to rehearse a release:

| Secret | Required | Description |
|--------|----------|-------------|
| `RPM_GPG_PRIVATE_KEY` | yes | Armored export of the PCP maintainers secret key |
| `RPM_GPG_PASSPHRASE` | no | Passphrase for that key, if it is protected |

The `pcp-masters` key is passphrase protected, so both secrets are needed
for an upstream release.

Export and upload the key with:

```bash
gpg --armor --export-secret-keys D87805D281D85A13539CB4BFA51D3ADCDEA2C114 > /tmp/pcp-masters.asc
gh secret set RPM_GPG_PRIVATE_KEY --repo performancecopilot/pcp < /tmp/pcp-masters.asc
gh secret set RPM_GPG_PASSPHRASE --repo performancecopilot/pcp
shred -u /tmp/pcp-masters.asc
```

The workflow imports the key into a temporary `GNUPGHOME`, asserts that
its fingerprint matches the value above, and removes it when the step
finishes.  A missing secret, a wrong key or a failed signature check
fails the release job before anything is uploaded.

### Rehearsing a release

Run the workflow manually with `dry_run` enabled.  It builds and signs the
artifacts exactly like a tagged release, skips the Packagecloud upload and
publishes the signed packages as the `signed-artifacts` workflow artifact:

```bash
gh workflow run release.yml --repo performancecopilot/pcp -f dry_run=true
```

### Verifying release packages

RPM verifies package signatures during installation from Fedora 45 onward.
Check a downloaded package explicitly with:

```bash
gpg --keyserver hkps://keyserver.ubuntu.com --recv-keys D87805D281D85A13539CB4BFA51D3ADCDEA2C114
gpg --export D87805D281D85A13539CB4BFA51D3ADCDEA2C114 | sudo rpm --import
rpm --checksig pcp-7.2.3-1.fc45.x86_64.rpm
```

apt and dpkg do not check signatures embedded in `.deb` packages
automatically, so verify those separately (debsigs 0.2.2 or later):

```bash
debsigs --verify pcp_7.2.3_amd64.deb
```

### Signing local builds

The scripts can be used on a local build too.  They expect `GNUPGHOME` to
contain the secret key, sign the packages in place and immediately verify
the result:

```bash
export GNUPGHOME=~/.gnupg
build/ci/sign-rpms.sh -k D87805D281D85A13539CB4BFA51D3ADCDEA2C114 build/rpm/RPMS/*/*.rpm
build/ci/sign-debs.sh -k D87805D281D85A13539CB4BFA51D3ADCDEA2C114 build/deb/*.deb
```

Use `-p passphrase-file` for a protected key and `-c` to verify only.
The `--signed` option of `Makepkgs` with `RPM_SIGN_KEYID` and/or
`DEB_SIGN_KEYID` remains the local release-manager path.

The [Signing self-test workflow](.github/workflows/sign-test.yml) runs the
same scripts against a throwaway RPM and DEB whenever they or the signing
container change.

## Deleting old versions to save storage space
We have limited space using the free packagecloud account option [1], so may need to respond to requests to free up used space from old builds.  This is done through the web interace.

[1] https://packagecloud.io/performancecopilot/pcp
