# macOS Development with Tart VMs

## Overview

Developing PCP on MacOS has some quirks. 
We provide Tart VM/CirrusLabs CLI configuration to allow an isolated macOS virtual machine for reproducible, clean-room builds that match the CI environment.

**Why Tart VMs?**
- macOS cannot be containerized (no Podman support)
- Lightweight, fast virtualization using native macOS frameworks
- Eliminates environment differences between developers
- Matches GitHub Actions CI as close as possible

## Prerequisites

Install Tart and Cirrus CLI:

```bash
brew install cirruslabs/cli/tart cirruslabs/cli/cirrus
```

## Basic Usage

Build PCP in a fresh VM:

```bash
cirrus run --dirty
```

Note: The `--dirty` flag is required to preserve executable permissions when copying the git tree to the VM.

## Useful Options

### Simpler Output

Use `--output simple` to see the complete build log (by default it is very concise):

```bash
cirrus run --dirty --output simple
```

### Skip Build (Reuse Existing Package)

If a valid macOS PKG already exists from a previous successful build, skip the build phase (you'll save 5 minutes!):

```bash
cirrus run --dirty -e PCP_SKIP_BUILD=true
```

Useful for testing installation without waiting for a full rebuild.

### Debug in VM via SSH

Pause the VM after installation to explore interactively:

```bash
cirrus run --dirty -e PCP_PAUSE_AFTER_INSTALL=true
```

The build log will show the VM IP address. Connect with:

```bash
ssh admin@<ip-address>  # Password: admin
```

The VM stays alive for up to 1 hour. Press `CTRL-C` to terminate when finished.

## Understanding .cirrus.yml

The `.cirrus.yml` file defines the build task:

- **Homebrew cache**: Caches `/opt/homebrew` to speed up subsequent builds
- **Build script**: Runs `./Makepkgs --verbose` (skipped if `PCP_SKIP_BUILD=true`)
- **Install script**: Mounts the generated DMG and installs the PKG
- **Verification**: Waits for pmcd service to start, validates installation
- **Pause script**: Optionally pauses VM if `PCP_PAUSE_AFTER_INSTALL=true`

See [.cirrus.yml](../../.cirrus.yml) for implementation details.

## Documentation

- [Tart Virtualization](https://tart.run/) - Official documentation
- [Tart on GitHub](https://github.com/cirruslabs/tart) - Source and issues
- [Tart Quick Start](https://tart.run/quick-start/) - Getting started
- [Cirrus CLI Integration](https://tart.run/integrations/cirrus-cli/) - How Cirrus CLI works with Tart
