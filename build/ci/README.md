# Cloud-based Continuous Integration

## Overview

Tests run on prepared images containing all required PCP build dependencies and shellscripts to build, install and run the PCP testsuite.

## Images

Images are created using [Packer](https://www.packer.io/).
Individual image configurations are stored in `hosts/`.
Common configuration is stored in `common/` (for example Debian and Ubuntu can share the same set of scripts).
Each image contains the following shell scripts:
* `/usr/local/ci/build.sh` to build PCP and move build artifacts to `~/artifacts`
* `/usr/local/ci/install.sh` to install PCP using the built artifacts from `~/artifacts`
* `/usr/local/ci/test.sh` to run a single test from the PCP testsuite

## Cluster management

The `scripts/` folder contains the following scripts:
* `build_image.sh` build a new image
* `start_vmss.sh` start a Virtual Machine Scale Set (a set of identical Virtual Machines)
* `start_build.sh` start the build on a single VM
* `start_install.sh` install PCP on all VMs in the scale set
* `start_tests.sh` start distributed tests on all VMs
* `stop_vmss.sh` stop all VMs

## Debugging

### Qemu
```
"qemuargs": [
  ["-serial", "stdio"]
]
```
