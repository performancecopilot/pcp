.. _nix-packaging:

Building PCP with Nix
=====================

This guide documents how to build Performance Co-Pilot (PCP) using the Nix
package manager and the NixOS Linux distribution.

.. important::

   This documentation explains why ``flake.nix`` was created and the technical
   decisions behind it, to help others maintain and update the Nix packaging in
   the future. If/when a PCP package is added to nixpkgs, this document will
   also serve as a reference for that Nix configuration.

What is Nix?
------------

`Nix <https://nixos.org/>`_ is a purely functional package manager that provides
reproducible, declarative, and reliable software builds. Key features include:

- **Reproducibility**: Builds are isolated and deterministic - the same inputs
  always produce the same outputs, regardless of the host system state.
- **Atomic upgrades and rollbacks**: Packages are installed in isolation,
  allowing instant rollback to previous versions.
- **Multiple versions**: Different versions of the same package can coexist
  without conflicts.
- **NixOS**: A Linux distribution built entirely on Nix, where the entire
  operating system configuration is declarative and reproducible.

Nix stores all packages in ``/nix/store/`` with cryptographic hashes in the
path (e.g., ``/nix/store/abc123...-pcp-7.0.5/``), ensuring complete isolation
between package versions and configurations.

For more information, visit:

- `Nix website <https://nixos.org/>`_
- `Nix manual <https://nixos.org/manual/nix/stable/>`_
- `Nixpkgs repository <https://github.com/NixOS/nixpkgs>`_

.. contents:: Table of Contents
   :depth: 3
   :local:

Introduction
------------

PCP can be built using the Nix package manager, providing reproducible builds
and easy dependency management. This documentation covers:

- Using the flake to build from source
- Available build options and feature flags
- MicroVM variants for development and testing
- Lifecycle testing framework for fine-grained VM validation
- Technical details of Nix-specific patches
- Known limitations and future improvements

The Nix packaging work was initiated to bring PCP into the `nixpkgs
<https://github.com/NixOS/nixpkgs>`_ repository, enabling NixOS users to easily
install and run PCP.

Modular Architecture
--------------------

The Nix packaging uses a modular design with separate files for different concerns.
This separation makes the codebase easier to maintain, test, and extend.

File Structure
^^^^^^^^^^^^^^

::

    nix/
    ├── package.nix           # PCP derivation (version from VERSION.pcp)
    ├── nixos-module.nix      # NixOS module for services.pcp
    ├── constants.nix         # Shared configuration constants
    ├── flake.nix             # Orchestrator (at repo root)
    │
    ├── bpf.nix               # BPF PMDA module (pre-compiled eBPF)
    ├── bcc.nix               # BCC PMDA module (DEPRECATED)
    │
    ├── microvm.nix           # Parametric MicroVM configuration (all variants)
    ├── microvm-scripts.nix   # VM management scripts (check, stop, ssh)
    ├── pmie-test.nix         # pmie testing module (stress-ng workload)
    │
    ├── container.nix         # OCI container image
    ├── network-setup.nix     # TAP/bridge network scripts
    ├── shell.nix             # Development shell
    ├── vm-test.nix           # NixOS VM integration test
    ├── test-lib.nix          # Shared test functions
    │
    ├── patches/              # Nix-specific patches
    │   ├── gnumakefile-nix-fixes.patch
    │   ├── python-libpcp-nix.patch
    │   ├── python-pmapi-no-reconnect.patch
    │   └── shell-portable-pwd.patch
    │
    ├── lifecycle/            # Modular lifecycle testing framework
    │   ├── default.nix       # Entry point, generates scripts for all variants
    │   ├── constants.nix     # Lifecycle-specific configuration
    │   ├── lib.nix           # Script generators (polling, connections, phases)
    │   ├── pcp-checks.nix    # PCP-specific verification (services, metrics)
    │   └── scripts/          # Expect scripts for console interaction
    │       ├── vm-expect.exp
    │       ├── vm-debug.exp
    │       └── vm-verify-pcp.exp
    │
    └── tests/
        ├── microvm-test.nix      # MicroVM test script builder
        └── test-all-microvms.nix # Comprehensive test runner

Module Descriptions
^^^^^^^^^^^^^^^^^^^

.. list-table::
   :header-rows: 1
   :widths: 25 75

   * - File
     - Purpose
   * - ``package.nix``
     - PCP derivation with all build logic. Parses version from ``VERSION.pcp``,
       applies patches, configures features, and wraps Python scripts for NixOS.
   * - ``nixos-module.nix``
     - NixOS module providing ``services.pcp`` options. Configures systemd
       services (pmcd, pmlogger, pmie, pmproxy), tmpfiles, user/group, and firewall.
   * - ``constants.nix``
     - Central configuration constants (ports, network IPs, VM resources, PMDA
       domain IDs, serial console ports, test thresholds). Imported by other
       modules to ensure consistency.
   * - ``bpf.nix``
     - NixOS module for pre-compiled BPF PMDA (pmdabpf). Uses CO-RE eBPF programs
       that load quickly without runtime compilation. Low memory (~512MB).
   * - ``bcc.nix``
     - NixOS module for BCC PMDA (pmdabcc). **DEPRECATED** - use pmdabpf instead.
       BCC used runtime eBPF compilation which is slower and less reliable than
       the pre-compiled BPF PMDA CO-RE approach.
   * - ``microvm.nix``
     - Parametric MicroVM configuration. Single module handles all variants via
       parameters (networking, debugMode, enableEvalTools, enableGrafana, etc.).
   * - ``microvm-scripts.nix``
     - VM management scripts that work with ALL variants: ``pcp-vm-check`` (list),
       ``pcp-vm-stop`` (stop), ``pcp-vm-ssh`` (connect). Detects VMs by hostname.
   * - ``pmie-test.nix``
     - Synthetic workload module: stress-ng service + dedicated pmie instance
       with rules to detect CPU spikes and log alerts.
   * - ``container.nix``
     - OCI container image with layered build for Docker/Podman deployment.
       Runs as non-root ``pcp`` user (UID 990).
   * - ``network-setup.nix``
     - Scripts to create/destroy TAP networking: bridge, NAT rules, vhost-net
       permissions for direct VM network access.
   * - ``shell.nix``
     - Development shell with PCP build dependencies plus debugging tools
       (gdb, valgrind on Linux, lldb on macOS).
   * - ``vm-test.nix``
     - NixOS VM integration test using ``pkgs.testers.nixosTest``. Verifies
       services start and metrics are queryable.
   * - ``test-lib.nix``
     - Shared bash functions for MicroVM tests: SSH polling, service checks,
       metric verification, security analysis.
   * - ``lifecycle/``
     - Modular lifecycle testing framework for fine-grained MicroVM validation.
       Provides individual phase scripts (build, process, console, services,
       metrics, shutdown) and full lifecycle tests per variant.

Quick Start
-----------

Building with Flakes (Recommended)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

From the PCP repository root::

    # Build PCP package
    nix build

    # Run pminfo
    nix run .#pcp -- pminfo --version

    # Enter a development shell (includes gdb, valgrind)
    nix develop

    # Run the NixOS VM integration test
    nix flake check

MicroVM Quick Start
^^^^^^^^^^^^^^^^^^^

Build and run a MicroVM for local testing (all variants have password SSH enabled)::

    # Build evaluation VM (includes pmie testing, node_exporter, below)
    nix build .#pcp-microvm-eval -o result-eval

    # Or build with pre-compiled BPF PMDA for eBPF metrics
    nix build .#pcp-microvm-bpf -o result-bpf

    # Start the VM (runs in foreground)
    ./result-eval/bin/microvm-run

    # In another terminal, manage the VM:
    nix run .#pcp-vm-ssh      # SSH into VM as root (password: pcp)
    nix run .#pcp-vm-check    # List running PCP MicroVMs
    nix run .#pcp-vm-stop     # Stop all running PCP MicroVMs

See :ref:`nixos-microvms` for full MicroVM documentation.

OCI Container Quick Start
^^^^^^^^^^^^^^^^^^^^^^^^^

Build and run PCP in a container::

    # Build the container image
    nix build .#pcp-container

    # Load into Docker
    docker load < result

    # Run pmcd (exposes ports 44321, 44322)
    docker run -d -p 44321:44321 -p 44322:44322 --name pcp pcp:latest

    # Query metrics from host
    pminfo -h localhost kernel.all.load

Flake Outputs Reference
^^^^^^^^^^^^^^^^^^^^^^^

.. list-table::
   :header-rows: 1
   :widths: 35 65

   * - Output
     - Description
   * - **Packages**
     -
   * - ``pcp`` (default)
     - PCP package built from source
   * - **Base VMs** (pmcd, pmlogger, pmproxy)
     -
   * - ``pcp-microvm``
     - Base MicroVM (user networking)
   * - ``pcp-microvm-tap``
     - Base MicroVM with TAP networking
   * - **Evaluation VMs** (+ node_exporter, below, pmie testing)
     -
   * - ``pcp-microvm-eval``
     - Evaluation VM (user networking)
   * - ``pcp-microvm-eval-tap``
     - Evaluation VM with TAP networking
   * - **Grafana VMs** (+ Prometheus + Grafana dashboards)
     -
   * - ``pcp-microvm-grafana``
     - Grafana VM (user networking, localhost:13000)
   * - ``pcp-microvm-grafana-tap``
     - Grafana VM with TAP networking (10.177.0.20:3000)
   * - **eBPF VMs** (kernel tracing)
     -
   * - ``pcp-microvm-bpf``
     - Pre-compiled BPF PMDA (fast startup, 1GB)
   * - **Other**
     -
   * - ``pcp-container``
     - OCI container image for Docker/Podman
   * - **Apps**
     -
   * - ``pcp-vm-ssh``
     - SSH into running MicroVM
   * - ``pcp-vm-stop``
     - Stop all running MicroVMs
   * - ``pcp-vm-check``
     - List running MicroVMs
   * - ``pcp-network-setup``
     - Create TAP bridge and NAT rules
   * - ``pcp-network-teardown``
     - Remove TAP bridge and NAT rules
   * - ``pcp-check-host``
     - Verify host environment for TAP networking
   * - ``pcp-test-base-user``
     - Test base VM (user networking)
   * - ``pcp-test-base-tap``
     - Test base VM (TAP networking)
   * - ``pcp-test-eval-user``
     - Test eval VM (user networking)
   * - ``pcp-test-eval-tap``
     - Test eval VM (TAP networking)
   * - **Lifecycle Testing**
     -
   * - ``pcp-lifecycle-full-test-<variant>``
     - Full lifecycle test for variant (base, eval, grafana, grafana-tap, bpf)
   * - ``pcp-lifecycle-test-all``
     - Test all variants sequentially (TAP skipped if network not set up)
   * - ``pcp-lifecycle-status-<variant>``
     - Check VM status (process, consoles, SSH)
   * - ``pcp-lifecycle-force-kill-<variant>``
     - Force kill a stuck VM
   * - **Testing - Run All**
     -
   * - ``pcp-test-all``
     - Run all tests sequentially (container + k8s + microvm)
   * - **Container Testing**
     -
   * - ``pcp-container-test``
     - Full container lifecycle test (build, run, verify, cleanup)
   * - ``pcp-container-test-quick``
     - Quick test (skip build, assume image loaded)
   * - **Kubernetes Testing**
     -
   * - ``pcp-k8s-test``
     - Full K8s DaemonSet lifecycle test (requires minikube)
   * - ``pcp-k8s-test-quick``
     - Quick test (skip build, assume image loaded)
   * - ``pcp-minikube-start``
     - Start minikube with optimal settings for PCP testing
   * - **Checks**
     -
   * - ``vm-test``
     - NixOS VM integration test

All MicroVM variants have ``debugMode=true`` by default, enabling password SSH
(root:pcp) for interactive testing convenience.

Feature Flags
-------------

Default Features
^^^^^^^^^^^^^^^^

The following features are enabled by default:

- **Core PCP tools and libraries**: pminfo, pmstat, pmrep, pmcd, etc.
- **Python3 and Perl language bindings**
- **Secure sockets**: SSL/TLS via OpenSSL
- **Service discovery**: via Avahi/mDNS
- **Transparent archive decompression**: via xz/lzma
- **Systemd integration**: unit files, tmpfiles, sysusers
- **Performance events**: via libpfm4
- **BPF PMDA**: kernel tracing via pre-compiled CO-RE eBPF
- **SNMP PMDA**: network device monitoring
- **Device mapper metrics**: LVM thin/cache via lvm2
- **RRDtool Perl bindings**: for RRD-based PMDAs

Optional Features
^^^^^^^^^^^^^^^^^

These can be enabled by overriding the nixpkgs package:

.. list-table::
   :header-rows: 1
   :widths: 30 70

   * - Flag
     - Description
   * - ``withQt = true``
     - Qt5 GUI tools (pmchart, pmview, etc.)
   * - ``withAmdgpu = true``
     - AMD GPU metrics (requires libdrm)
   * - ``withInfiniband = true``
     - Infiniband fabric metrics (requires rdma-core)
   * - ``withSelinux = true``
     - SELinux policy support
   * - ``withPythonExport = true``
     - pcp2arrow (pyarrow), pcp2xlsx (openpyxl)
   * - ``withPythonDatabase = true``
     - pmdapostgresql, pmdamongodb
   * - ``withPythonInfra = true``
     - pmdalibvirt (requires libvirt, lxml)
   * - ``withPerlDatabase = true``
     - DBI for pmdaoracle, pmdamysql
   * - ``withPerlNetwork = true``
     - Net::SNMP, Cache::Memcached, XML::LibXML
   * - ``withPerlMisc = true``
     - Date::Manip, Spreadsheet::WriteExcel

Example with overrides (nixpkgs only)::

    nix-build -A pcp.override { withQt = true; withPythonExport = true; }

.. note::

   The in-tree ``flake.nix`` provides a simplified build with default features
   enabled (BPF, systemd, SNMP, etc.) but does not expose all optional flags.
   For full customization, use the nixpkgs package with overrides.

PMDAs Requiring Unavailable Dependencies
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Some PMDAs require Python/Perl packages not yet available in nixpkgs:

- **pcp2spark**: requires pyspark
- **pmdamssql**: requires pyodbc + unixODBC setup
- **pmdalio**: requires rtslib-fb
- **pmdahdb**: requires hdbcli/pyhdbcli (SAP HANA)
- **pmdaslurm**: requires Slurm::Sacctmgr Perl module

Package Output Structure
------------------------

After building with ``nix build``, the ``result`` symlink points to the package
in the Nix store. The package follows a modified FHS-like structure:

::

    result/
    ├── bin/                    # User-facing CLI tools
    │   ├── pminfo              # Query metric information
    │   ├── pmstat              # System performance overview
    │   ├── pmrep               # Customizable reporting
    │   ├── pmlogger            # Archive performance data
    │   ├── pmie                # Inference engine
    │   ├── pcp2*               # Export tools (json, xml, elasticsearch, etc.)
    │   └── ...                 # ~70 tools total
    │
    ├── lib/                    # Shared libraries
    │   ├── libpcp.so.*         # Core PCP library
    │   ├── libpcp_pmda.so.*    # PMDA development library
    │   ├── libpcp_web.so.*     # Web/REST API support
    │   ├── libpcp_gui.so.*     # GUI toolkit integration
    │   ├── python3.*/          # Python bindings (pcp module)
    │   └── perl5/              # Perl bindings (PCP::PMDA, etc.)
    │
    ├── libexec/pcp/            # Internal executables
    │   ├── bin/                # pmcd, pmproxy, pmnsmerge, etc.
    │   ├── pmdas/              # PMDA binaries and scripts
    │   ├── pmns/               # PMNS source files (root_*)
    │   └── services/           # Service control scripts
    │
    ├── share/pcp/              # Shared data and config templates
    │   ├── etc/                # Configuration files (moved from /etc)
    │   │   ├── pcp.conf        # Main PCP configuration
    │   │   └── pcp/            # Per-service configs (pmcd, pmlogger, etc.)
    │   ├── lib/                # Shell function libraries
    │   ├── demos/              # Example code and tutorials
    │   └── htop/               # pcp-htop configuration
    │
    ├── var/lib/pcp/            # Variable data (static portions only)
    │   ├── pmns/               # Combined PMNS root file
    │   ├── pmdas/              # PMDA installation state
    │   └── config/             # Runtime configuration templates
    │
    └── include/                # C header files for development
        └── pcp/                # pcp/*.h headers

**Key differences from traditional installation:**

- Configuration moved from ``/etc`` to ``share/pcp/etc`` (NixOS module
  materializes to ``/etc`` at activation time)
- Runtime state directories (``/var/run``, ``/var/log``) removed (created
  by systemd tmpfiles at runtime)
- All paths are absolute to the Nix store (``/nix/store/<hash>-pcp-x.y.z/``)

Technical Details
-----------------

This section documents the Nix-specific patches and workarounds required to
build PCP in the Nix sandbox environment.

Why Nix Packaging Can Be Challenging
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Nix's approach to reproducible builds imposes constraints that differ
significantly from traditional Linux distributions. These constraints often
expose assumptions and hardcoded values in build systems that would otherwise
go unnoticed:

**Non-standard Filesystem Hierarchy**
  Nix does not follow the Filesystem Hierarchy Standard (FHS). Binaries,
  libraries, and configuration files live in ``/nix/store/<hash>-<name>/``
  rather than ``/usr/bin``, ``/usr/lib``, or ``/etc``. This breaks:

  - Hardcoded paths like ``/usr/bin/ar`` or ``/usr/lib/tmpfiles.d``
  - Scripts that assume tools are in ``PATH`` at standard locations
  - Build systems that probe for dependencies in fixed paths

**Sandboxed Builds**
  Nix builds run in a sandbox with restricted filesystem access. The build
  environment cannot write to locations like ``/tmp`` or ``/var/tmp`` - instead,
  ``$TMPDIR`` points to an isolated temporary directory. This reveals:

  - Hardcoded temporary paths (e.g., ``/var/tmp`` in PCP's ``mk.rewrite`` scripts)
  - Assumptions about writable system directories
  - Network access attempts during builds (blocked by default)

**Pure Build Environment**
  Each build starts with a minimal, reproducible environment. There are no
  implicit dependencies from the host system:

  - ``pkg-config`` returns paths inside Nix store (read-only for other packages)
  - Compilers include hardening flags that may conflict with specialized targets
    (e.g., BPF's ``-target bpf`` doesn't support ``-fstack-protector``)
  - Environment variables like ``PATH`` contain only explicitly declared inputs

**Static Analysis Benefits**
  While these constraints require extra packaging work, they provide significant
  benefits:

  - Exposes portability issues and implicit dependencies
  - Forces explicit declaration of all build requirements
  - Catches hardcoded paths that may break on other systems
  - Results in packages that are more likely to build correctly on any platform

The patches documented below address these Nix-specific challenges for PCP,
and many would benefit upstream by improving portability across all platforms.

BPF Compilation
^^^^^^^^^^^^^^^

PCP's BPF PMDAs (pmdabpf, pmdabcc, pmdabpftrace) compile ``.bpf.c`` files to
BPF bytecode using clang with ``-target bpf``. Nix's wrapped clang adds
hardening flags that the BPF backend doesn't support::

    clang: error: unsupported option '-fzero-call-used-regs=used-gpr' for target 'bpf'
    clang: warning: ignoring '-fstack-protector-strong' option as it is not
                    currently supported for target 'bpf'

**Solution**:

1. Disable the unsupported hardening flag::

       hardeningDisable = [ "zerocallusedregs" ];

2. Suppress stack protector warnings via ``BPF_CFLAGS``::

       BPF_CFLAGS = "-fno-stack-protector -Wno-error=unused-command-line-argument";

3. Use ``lib.getExe`` to get the wrapped clang path::

       CLANG = lib.getExe llvmPackages.clang;

This approach was adapted from the ``xdp-tools`` package in nixpkgs, which
faces similar BPF compilation challenges.

.. _bpf-vs-bcc:

BPF vs BCC PMDAs
^^^^^^^^^^^^^^^^

PCP provides two approaches for eBPF-based metrics collection:

**pmdabpf (Pre-compiled CO-RE eBPF)** - Recommended:

- Uses pre-compiled CO-RE (Compile Once, Run Everywhere) eBPF bytecode
- Fast startup: No runtime compilation needed
- Low memory: ~512MB VM (no clang/LLVM required)
- Requires BTF-enabled kernel (``CONFIG_DEBUG_INFO_BTF=y``)
- Available modules: biolatency, runqlat, netatop, oomkill, execsnoop, exitsnoop,
  opensnoop, vfsstat, tcpconnlat, tcpconnect, biosnoop, fsslower, statsnoop,
  mountsnoop, bashreadline

The BPF PMDA provides metrics like ``bpf.runq.latency`` (scheduler run queue
latency histogram) and ``bpf.disk.all.latency`` (block I/O latency histogram).
Additional modules can be enabled in ``bpf.conf``.

**pmdabcc (Runtime BCC eBPF)** - **DEPRECATED**:

.. warning::

   BCC PMDA is deprecated upstream and will be removed in a future PCP release.
   Use pmdabpf instead. The ``pcp-microvm-bcc`` variant has been removed.

   From pmdabcc(1): "This PMDA is now deprecated and will be removed in a
   future release, transition to using its replacement pmdabpf(1) instead."

**Enabling in MicroVM:**

For pmdabpf::

    nix build .#pcp-microvm-bpf

Nix Sandbox /var/tmp Fix
^^^^^^^^^^^^^^^^^^^^^^^^

The Nix build sandbox restricts filesystem access. Several PCP build scripts
hardcode ``/var/tmp`` for temporary files, causing errors::

    ./mk.rewrite: line 12: /var/tmp/37183.c: No such file or directory

**Affected scripts**:

- ``src/pmdas/bind2/mk.rewrite``
- ``src/pmdas/jbd2/mk.rewrite``
- ``src/pmdas/linux/mk.rewrite``
- ``src/pmdas/linux_proc/mk.rewrite``
- ``src/bashrc/getargs``
- ``src/libpcp/src/check-errorcodes``
- ``src/libpcp3/src/check-errorcodes``
- ``src/libpcp/doc/mk.cgraph``
- ``src/pmlogcompress/check-optimize``

**Solution**: The patch file ``nix/patches/tmpdir-portability.patch`` replaces
``/var/tmp`` with ``${TMPDIR:-/tmp}`` in all affected scripts. This is portable:

- On Nix, ``$TMPDIR`` is set to a writable sandbox directory
- On traditional systems, it falls back to ``/tmp``

This fix could be submitted upstream to improve portability across all build
environments.

Systemd Integration Paths
^^^^^^^^^^^^^^^^^^^^^^^^^

PCP uses ``pkg-config`` to find systemd directories from ``systemd.pc``. On
NixOS, this returns paths inside the systemd store path (read-only)::

    mkdir: cannot create directory '/nix/store/...-systemd-.../lib/systemd/system': Permission denied

Additionally, PCP's ``GNUmakefile`` has a hardcoded fallback to
``/usr/lib/tmpfiles.d``.

**Solutions**:

1. Override paths via environment variables::

       SYSTEMD_SYSTEMUNITDIR = "${placeholder "out"}/lib/systemd/system";
       SYSTEMD_TMPFILESDIR = "${placeholder "out"}/lib/tmpfiles.d";
       SYSTEMD_SYSUSERSDIR = "${placeholder "out"}/lib/sysusers.d";

2. The patch file ``nix/patches/gnumakefile-nix-fixes.patch`` redirects the
   tmpfiles installation from ``/usr/lib/tmpfiles.d`` to
   ``$(PCP_SHARE_DIR)/tmpfiles.d``.

Configure Script Fixes
^^^^^^^^^^^^^^^^^^^^^^

PCP's configure script has a hardcoded fallback for the ``ar`` archiver::

    test -n "$AR" || AR="/usr/bin/ar"

On NixOS, ``/usr/bin/ar`` doesn't exist.

**Solution**: Export ``AR`` in ``preConfigure`` before configure runs::

    preConfigure = ''
      export AR="${stdenv.cc.bintools}/bin/ar"
    '';

Since ``AR`` is already set when configure runs, the hardcoded fallback is never
reached. No patch is needed - the ``preConfigure`` export handles it completely.

.. note::

   The patch file ``nix/patches/configure-ar-portable.patch`` exists but is not
   applied in ``flake.nix``. It changes the fallback to ``ar`` (PATH lookup)
   and could be submitted upstream to improve portability for non-Nix builds.

Install Ownership
^^^^^^^^^^^^^^^^^

PCP's ``GNUmakefile`` uses ``-o $(PCP_USER) -g $(PCP_GROUP)`` flags with
``install(1)`` to set file ownership. This requires root privileges and fails
in the Nix sandbox::

    install: cannot change ownership: Operation not permitted

**Solution**: The patch file ``nix/patches/gnumakefile-nix-fixes.patch`` removes
the ownership flags from install commands. The NixOS module (when created) will
handle proper ownership at activation time.

Broken Symlinks Fix
^^^^^^^^^^^^^^^^^^^

PCP creates some symlinks with malformed targets containing double path
prefixes like ``/nix/store/nix/store/...``. This happens when the build system
prepends a path to an already-absolute path.

**Affected symlinks** (after ``/etc`` is moved to ``share/pcp/etc/``):

- ``share/pcp/etc/pcp/pmsearch/pmsearch.conf`` → should point to ``pmproxy/pmproxy.conf``
- ``share/pcp/etc/pcp/pmseries/pmseries.conf`` → should point to ``pmproxy/pmproxy.conf``
- ``share/pcp/etc/pcp/pmcd/rc.local`` → should point to ``libexec/pcp/services/local``

**Solution**: Fix in postInstall by removing and recreating with correct targets::

    # Fix pmsearch/pmseries config symlinks
    for broken_link in "$out/share/pcp/etc/pcp/pm"{search/pmsearch,series/pmseries}.conf; do
      [[ -L "$broken_link" ]] && rm "$broken_link" && \
        ln -sf "$out/share/pcp/etc/pcp/pmproxy/pmproxy.conf" "$broken_link"
    done

    # Fix pmcd/rc.local symlink
    if [[ -L "$out/share/pcp/etc/pcp/pmcd/rc.local" ]]; then
      rm "$out/share/pcp/etc/pcp/pmcd/rc.local"
      ln -sf "$out/libexec/pcp/services/local" "$out/share/pcp/etc/pcp/pmcd/rc.local"
    fi

Patch Files
^^^^^^^^^^^

The Nix-specific fixes have been implemented as proper ``.patch`` files in
``nix/patches/`` rather than inline ``substituteInPlace`` calls:

**Applied in flake.nix:**

- ``gnumakefile-nix-fixes.patch`` - Ownership flags, tmpfiles path, qa exclusion
- ``tmpdir-portability.patch`` - Use ``${TMPDIR:-/tmp}`` instead of ``/var/tmp``

**Available but not applied** (handled by ``preConfigure`` instead):

- ``configure-ar-portable.patch`` - Portable ar fallback (useful for upstream)

Benefits of using patch files:

- Patches fail loudly when they don't apply (easier version upgrades)
- Easier to submit upstream for review
- Clear documentation of what changes are needed
- Better for code review

Only ``patchShebangs`` remains in ``postPatch`` as it requires Nix store paths.

Package Outputs
---------------

The Nix package is split into multiple outputs to reduce closure size:

.. list-table::
   :header-rows: 1
   :widths: 15 85

   * - Output
     - Contents
   * - ``out``
     - Main binaries, libraries, PMDAs, examples, configuration templates
   * - ``man``
     - Man pages (~15MB compressed)
   * - ``doc``
     - Books, tutorials, programmer's guides

Users who only need the binaries can install just ``out``, saving significant
disk space.

Testing
-------

QA Suite (Disabled)
^^^^^^^^^^^^^^^^^^^

The full QA test suite is disabled by default (``doCheck = false``) because:

1. Tests require a running ``pmcd`` daemon
2. The QA suite has thousands of scripts that significantly slow builds

The ``qa`` directory is patched out of ``SUBDIRS`` to prevent installation
entirely, avoiding slow shebang patching of test scripts.

NixOS VM Test
^^^^^^^^^^^^^

A NixOS VM integration test is provided in ``nix/vm-test.nix`` to verify the
package works correctly in a real NixOS environment. The test:

1. Boots a NixOS VM with PCP installed
2. Creates the required ``pcp`` user/group and runtime directories
3. Starts the ``pmcd`` daemon
4. Waits for pmcd to listen on port 44321
5. Queries ``pminfo -f kernel.all.load`` to verify metrics collection works

**Running the VM test**::

    # Run via flake check (runs all checks)
    nix flake check

    # Or build just the VM test
    nix build .#checks.x86_64-linux.vm-test

    # Then to view the logs
    nix log .#checks.x86_64-linux.vm-test

    # For aarch64 systems
    nix build .#checks.aarch64-linux.vm-test

Use ``-L`` to see test output in real-time::

    nix build .#checks.x86_64-linux.vm-test -L

**Example successful test output**::

    === Checking listening ports ===
    machine: must succeed: ss -tlnp | grep -E '44321|pmcd' || echo 'No pmcd ports found'
    machine: (finished: must succeed: ..., in 0.05 seconds)
    LISTEN 0  5  0.0.0.0:44321  0.0.0.0:*  users:(("pmcd",pid=947,fd=3))
    LISTEN 0  5     [::]:44321     [::]:*  users:(("pmcd",pid=947,fd=4))

    machine: waiting for TCP port 44321 on localhost
    machine # Connection to localhost (::1) 44321 port [tcp/pmcd] succeeded!
    machine: (finished: waiting for TCP port 44321 on localhost, in 0.05 seconds)

    machine: must succeed: PCP_CONF=/.../pcp.conf pminfo -f kernel.all.load
    machine: (finished: must succeed: ... pminfo -f kernel.all.load, in 0.04 seconds)

    machine: must succeed: PCP_CONF=/.../pcp.conf pminfo -h localhost kernel.all.cpu.user
    machine: (finished: must succeed: ... pminfo -h localhost kernel.all.cpu.user, in 0.03 seconds)

    === PCP VM test passed! ===
    (finished: run the VM test script, in 18.65 seconds)
    test script finished in 18.69s
    cleanup
    kill machine (pid 9)

**Running the VM interactively** (for debugging)::

    # Build the test driver
    nix build .#checks.x86_64-linux.vm-test.driverInteractive

    # Run the interactive VM
    ./result/bin/nixos-test-driver --interactive

    # Inside the Python REPL, you can run commands like:
    # >>> machine.succeed("pminfo --version")
    # >>> machine.shell_interact()  # Get a shell in the VM

**Test configuration** (from ``nix/vm-test.nix``):

The VM is configured with:

- PCP package installed via ``environment.systemPackages``
- ``pcp`` user/group created as a system user
- Runtime directories (``/var/lib/pcp``, ``/var/log/pcp``, ``/run/pcp``)
  created via systemd tmpfiles

MicroVM Tests
^^^^^^^^^^^^^

Interactive MicroVM tests provide more comprehensive validation than the
automated VM test. These tests verify services, metrics, HTTP endpoints,
journal health, and pmie functionality.

**Running MicroVM tests**::

    # Start a VM first
    nix build .#pcp-microvm-eval -o result-eval
    ./result-eval/bin/microvm-run &

    # Run the test suite (waits for VM to boot)
    nix run .#pcp-test-eval-user

    # Clean up
    nix run .#pcp-vm-stop

**Test phases** (from ``nix/tests/microvm-test.nix``):

1. **SSH connectivity** - Wait for VM to accept SSH connections
2. **Service status** - Verify pmcd, pmproxy, node_exporter are active
3. **PCP metrics** - Query kernel.all.load, cpu.user, mem.physmem
4. **HTTP endpoints** - Test pmproxy REST API and node_exporter
5. **Journal health** - Check for errors in service journals
6. **TUI smoke test** - Run ``pcp dstat`` briefly
7. **Metric parity** - Compare PCP vs node_exporter values (eval VMs)
8. **pmie testing** - Verify alerts.log has CPU elevation entries (eval VMs)

Test results are saved to ``test-results/<variant>/results.txt``.

.. seealso::

   For fine-grained, phase-by-phase MicroVM testing with individual control over
   build, boot, console, service, and metric verification phases, see the
   :ref:`lifecycle-testing` section under NixOS MicroVMs.

Running All Tests
^^^^^^^^^^^^^^^^^

The ``pcp-test-all`` command runs all test suites sequentially::

    nix run .#pcp-test-all

This executes three test suites in order:

1. **Container test** - Builds and tests PCP in Docker/Podman
2. **Kubernetes test** - Deploys PCP DaemonSet to minikube
3. **MicroVM tests** - Tests all MicroVM variants (skips TAP)

**Prerequisites:**

- Docker or Podman installed and running
- Minikube available (will be started automatically if not running)

**Individual test suites:**

.. list-table::
   :header-rows: 1
   :widths: 40 60

   * - Command
     - Description
   * - ``nix run .#pcp-container-test``
     - Container lifecycle only
   * - ``nix run .#pcp-k8s-test``
     - Kubernetes DaemonSet only
   * - ``nix run .#pcp-test-all-microvms``
     - All MicroVM variants only

**Quick tests** (skip build phase, faster iteration)::

    nix run .#pcp-container-test-quick
    nix run .#pcp-k8s-test-quick

.. _nixos-microvms:

NixOS MicroVMs
--------------

The Nix packaging includes MicroVM configurations for development, testing, and
evaluation. These lightweight virtual machines provide isolated PCP environments
that can be built and run entirely from the flake.

MicroVM variants are built using `microvm.nix <https://github.com/astro/microvm.nix>`_,
providing fast boot times (~2s) and efficient resource sharing via 9p filesystem.

MicroVM Variants (7 total)
^^^^^^^^^^^^^^^^^^^^^^^^^^

All variants have ``debugMode=true`` by default, enabling password SSH (root:pcp)
for interactive testing convenience.

.. list-table::
   :header-rows: 1
   :widths: 30 50 20

   * - Variant
     - Purpose
     - Memory
   * - ``pcp-microvm``
     - Base PCP (pmcd, pmlogger, pmproxy)
     - 1GB
   * - ``pcp-microvm-tap``
     - Base with TAP networking
     - 1GB
   * - ``pcp-microvm-eval``
     - Eval tools (node_exporter, below, pmie-test)
     - 1GB
   * - ``pcp-microvm-eval-tap``
     - Eval with TAP
     - 1GB
   * - ``pcp-microvm-grafana``
     - Full demo (Grafana + Prometheus + eval tools)
     - 1GB
   * - ``pcp-microvm-grafana-tap``
     - Full demo with TAP
     - 1GB
   * - ``pcp-microvm-bpf``
     - Pre-compiled eBPF (CO-RE, fast startup)
     - 1GB

**Choosing a variant:**

- Use ``pcp-microvm`` for basic PCP testing with archive logging
- Use ``pcp-microvm-eval`` for comparing PCP vs node_exporter
- Use ``pcp-microvm-grafana`` for visual demos with dashboards
- Use ``pcp-microvm-bpf`` for eBPF metrics (see :ref:`bpf-vs-bcc`)

.. note::

   The ``pcp-microvm-bcc`` variant has been removed. BCC PMDA is deprecated
   upstream - use pmdabpf instead.

Custom Variants
^^^^^^^^^^^^^^^

The ``mkMicroVM`` function in ``flake.nix`` accepts these parameters:

.. list-table::
   :header-rows: 1
   :widths: 25 15 60

   * - Parameter
     - Default
     - Description
   * - ``networking``
     - ``"user"``
     - ``"user"`` (port forwarding) or ``"tap"`` (direct network)
   * - ``debugMode``
     - ``true``
     - Enable password SSH (root:pcp)
   * - ``enablePmlogger``
     - ``true``
     - Enable archive logging
   * - ``enableEvalTools``
     - ``false``
     - Enable node_exporter + below
   * - ``enablePmieTest``
     - ``false``
     - Enable stress-ng workload + pmie rules
   * - ``enableGrafana``
     - ``false``
     - Enable Grafana + Prometheus
   * - ``enableBpf``
     - ``false``
     - Enable pre-compiled BPF PMDA (CO-RE eBPF)

Example custom variant in ``flake.nix``::

    pcp-microvm-custom = mkMicroVM {
      networking = "tap";
      enableEvalTools = true;
      enableGrafana = true;
      enableBpf = true;
    };

VM Management Scripts
^^^^^^^^^^^^^^^^^^^^^

Helper scripts are provided to manage running MicroVMs. These scripts work
with **all MicroVM variants** - they detect VMs by hostname pattern.

.. list-table::
   :header-rows: 1
   :widths: 40 60

   * - Command
     - Description
   * - ``nix run .#pcp-vm-check``
     - List all running PCP MicroVMs and show count
   * - ``nix run .#pcp-vm-ssh``
     - SSH into VM as root (password: pcp)
   * - ``nix run .#pcp-vm-ssh -- --variant=eval``
     - SSH to a specific variant (base, eval, grafana, bpf)
   * - ``nix run .#pcp-vm-stop``
     - Stop all running PCP MicroVMs (SIGTERM, then SIGKILL)
   * - ``nix run .#pcp-test-all-microvms``
     - Run comprehensive tests on all variants

The scripts detect VMs by matching the hostname pattern in the process list:
``pcp-vm``, ``pcp-eval-vm``, ``pcp-grafana-vm``, ``pcp-bpf-vm``.

**Port allocation** - Each variant uses unique ports to avoid conflicts:

.. list-table::
   :header-rows: 1
   :widths: 20 20 20 20 20

   * - Variant
     - SSH
     - pmcd
     - pmproxy
     - Offset
   * - base
     - 22022
     - 44321
     - 44322
     - 0
   * - eval
     - 22122
     - 44421
     - 44422
     - +100
   * - grafana
     - 22222
     - 44521
     - 44522
     - +200
   * - bpf
     - 22322
     - 44621
     - 44622
     - +300

Serial Console Debugging
^^^^^^^^^^^^^^^^^^^^^^^^

Each MicroVM exposes two serial consoles via TCP for debugging early boot issues
and network problems. These are invaluable when SSH is not available.

**Console types:**

- **ttyS0 (serial)**: Traditional UART console, slow but available immediately
  at boot. Use for debugging kernel boot, initrd, and early systemd issues.
- **hvc0 (virtio)**: High-speed virtio console, available after virtio drivers
  load. Faster for interactive use once the system is booted.

**Serial console port allocation:**

.. list-table::
   :header-rows: 1
   :widths: 20 25 25 30

   * - Variant
     - Serial (ttyS0)
     - Virtio (hvc0)
     - Description
   * - base
     - 24500
     - 24501
     - Base PCP VM
   * - eval
     - 24510
     - 24511
     - Evaluation VM
   * - grafana
     - 24520
     - 24521
     - Grafana VM
   * - bpf
     - 24530
     - 24531
     - BPF PMDA VM

**Connecting to serial consoles:**

Basic connection using netcat::

    # Connect to serial console (slow, early boot)
    nc localhost 24500

    # Connect to virtio console (fast, after boot)
    nc localhost 24501

For better terminal handling (proper line editing, raw mode)::

    # Using socat for raw terminal mode
    socat -,rawer tcp:localhost:24500

    # With escape sequence support (Ctrl-] to exit)
    socat -,rawer,escape=0x1d tcp:localhost:24500

**Debugging scenarios:**

1. **Kernel boot issues** - Connect to serial (ttyS0) before starting the VM
   to capture kernel boot messages from the very beginning::

       # In terminal 1: connect to serial first
       nc localhost 24500

       # In terminal 2: start the VM
       ./result/bin/microvm-run

2. **Network problems** - When SSH isn't working, use serial to investigate::

       # Connect via serial
       nc localhost 24500

       # Check network status inside VM
       ip addr show
       systemctl status network-online.target
       journalctl -u dhcpcd

3. **Service failures** - Use serial to check why pmcd or other services aren't
   starting::

       nc localhost 24500

       # Inside VM
       systemctl status pmcd
       journalctl -u pmcd -e

**Multiple VMs:**

All variants can run simultaneously without port conflicts. Each variant's
serial ports are offset by 10 from the base port (24500)::

    # Terminal 1: Base VM serial
    nc localhost 24500

    # Terminal 2: Eval VM serial
    nc localhost 24510

    # Terminal 3: Grafana VM serial
    nc localhost 24520

**Example workflow**::

    # Build and start any variant
    nix build .#pcp-microvm-grafana -o result
    ./result/bin/microvm-run &

    # Check it's running
    nix run .#pcp-vm-check

    # SSH into the VM
    nix run .#pcp-vm-ssh

    # Inside the VM, explore PCP
    pminfo -f kernel.all.load
    systemctl status pmcd

    # Exit SSH and stop the VM
    exit
    nix run .#pcp-vm-stop

The SSH script uses user-mode networking port forwarding. Use ``--variant=`` to
connect to the correct port for each variant. For TAP networking, SSH directly
to the VM IP (``ssh root@10.177.0.20``).

Comprehensive Test Runner
^^^^^^^^^^^^^^^^^^^^^^^^^

The ``pcp-test-all-microvms`` app builds and tests all MicroVM variants sequentially::

    # Test all variants (skip TAP if network not set up)
    nix run .#pcp-test-all-microvms -- --skip-tap

    # Test only a specific variant
    nix run .#pcp-test-all-microvms -- --only=grafana

    # Show help
    nix run .#pcp-test-all-microvms -- --help

The test runner:

- **Polling-based builds** - Polls every 10s for build completion (supports slow machines)
- **Sequential execution** - Builds one variant at a time to leverage Nix caching
- **Variant-specific checks** - Tests appropriate services for each variant
- **Continue on failure** - Reports all results at end instead of stopping on first failure

**Checks per variant:**

- **base**: pmcd, pmproxy, pmlogger
- **eval**: pmcd, pmproxy, node_exporter
- **grafana**: pmcd, pmproxy, node_exporter, Grafana HTTP, Prometheus HTTP, BPF metrics
- **grafana-tap**: Same as grafana, using TAP networking (direct IP: 10.177.0.20)
- **bpf**: pmcd, pmproxy, node_exporter, BPF metrics (runq.latency, disk.all.latency)

.. _lifecycle-testing:

Lifecycle Testing Framework
^^^^^^^^^^^^^^^^^^^^^^^^^^^

The lifecycle testing framework provides fine-grained control over MicroVM testing,
with individual phases that can be run separately for debugging. This is useful for
diagnosing boot issues, service failures, or metric collection problems.

**Full lifecycle test for a variant:**

::

    # Test a specific variant through all phases
    nix run .#pcp-lifecycle-full-test-base
    nix run .#pcp-lifecycle-full-test-eval
    nix run .#pcp-lifecycle-full-test-grafana
    nix run .#pcp-lifecycle-full-test-bpf

    # Test with TAP networking (requires host network setup first)
    nix run .#pcp-network-setup                   # Create TAP bridge
    nix run .#pcp-lifecycle-full-test-grafana-tap # Test grafana with direct IP
    nix run .#pcp-network-teardown                # Clean up when done

    # Test all variants sequentially
    nix run .#pcp-lifecycle-test-all

    # Test only specific variant
    nix run .#pcp-lifecycle-test-all -- --only=grafana
    nix run .#pcp-lifecycle-test-all -- --only=grafana-tap

**Lifecycle phases:**

.. list-table::
   :header-rows: 1
   :widths: 10 25 65

   * - Phase
     - Name
     - Description
   * - 0
     - Build VM
     - Build the MicroVM derivation via ``nix build``
   * - 1
     - Start VM
     - Start QEMU process and verify it's running
   * - 2
     - Serial Console
     - Verify serial console (ttyS0) is responsive
   * - 2b
     - Virtio Console
     - Verify virtio console (hvc0) is responsive
   * - 3
     - Verify Services
     - Check PCP and related services are active
   * - 4
     - Verify Metrics
     - Check PCP metrics are available via pminfo
   * - 5
     - Shutdown
     - Send shutdown command via console
   * - 6
     - Wait Exit
     - Wait for VM process to exit cleanly

**Utility scripts:**

::

    # Check VM status (process, consoles, SSH)
    nix run .#pcp-lifecycle-status-base
    nix run .#pcp-lifecycle-status-bpf

    # Force kill a stuck VM
    nix run .#pcp-lifecycle-force-kill-base

**Example output:**

::

    ========================================
      PCP MicroVM Full Lifecycle Test (base)
    ========================================

    Description: Base PCP (pmcd, pmlogger, pmproxy)
    Hostname: pcp-vm
    SSH Port: 22022

    --- Phase 0: Build VM (timeout: 600s) ---
      PASS: VM built (4066ms)

    --- Phase 1: Start VM (timeout: 5s) ---
      PASS: VM process running (PID: 12345) (54ms)

    --- Phase 2: Check Serial Console (timeout: 30s) ---
      PASS: Serial console available (port 24500) (6ms)

    --- Phase 2b: Check Virtio Console (timeout: 45s) ---
      PASS: Virtio console available (port 24501) (6ms)

    --- Phase 3: Verify PCP Services (timeout: 60s) ---
      SSH connected (2341ms)
      PASS: pmcd active (42ms)
      PASS: pmproxy active (38ms)
      PASS: pmlogger active (41ms)

    --- Phase 4: Verify PCP Metrics (timeout: 30s) ---
      PASS: kernel.all.load (52ms)
      PASS: kernel.all.cpu.user (48ms)
      PASS: mem.physmem (45ms)

    --- Phase 5: Shutdown (timeout: 30s) ---
      PASS: Shutdown command sent (123ms)

    --- Phase 6: Wait for Exit (timeout: 60s) ---
      PASS: VM exited cleanly (5234ms)

      Timing Summary
      ─────────────────────────────────────
      Phase                     Time (ms)
      ─────────────────────────────────────
      build                          4066
      start                            54
      serial                            6
      virtio                            6
      services                       2462
      metrics                         145
      shutdown                        123
      exit                           5234
      ─────────────────────────────────────
      TOTAL                         12096
      ─────────────────────────────────────

    ========================================
      Result: ALL PHASES PASSED
      Total time: 12.0s
    ========================================

**Full lifecycle test output (all variants):**

Running ``nix run .#pcp-lifecycle-test-all`` tests all 4 variants sequentially
and produces a summary report at the end::

    ========================================
      PCP MicroVM Full Lifecycle Test (base)
    ========================================

    Description: Base PCP (pmcd, pmlogger, pmproxy)
    Hostname: pcp-vm
    SSH Port: 22022

    --- Phase 0: Build VM (timeout: 600s) ---
      PASS: VM built (2669ms)

    --- Phase 1: Start VM (timeout: 5s) ---
      PASS: VM process running (PID: 2929155) (72ms)

    --- Phase 2: Check Serial Console (timeout: 30s) ---
      PASS: Serial console available (port 24500) (8ms)

    --- Phase 2b: Check Virtio Console (timeout: 45s) ---
      PASS: Virtio console available (port 24501) (7ms)

    --- Phase 3: Verify PCP Services (timeout: 60s) ---
      SSH connected (19063ms)
      PASS: pmcd active (160ms)
      PASS: pmproxy active (141ms)
      PASS: pmlogger active (140ms)

    --- Phase 4: Verify PCP Metrics (timeout: 30s) ---
      PASS: kernel.all.load (235ms)
      PASS: kernel.all.cpu.user (129ms)
      PASS: mem.physmem (127ms)

    --- Phase 5: Shutdown (timeout: 30s) ---
      PASS: Shutdown command sent (142ms)

    --- Phase 6: Wait for Exit (timeout: 60s) ---
      PASS: VM exited cleanly (13284ms)

      Timing Summary
      ─────────────────────────────────────
      Phase                     Time (ms)
      ─────────────────────────────────────
      build                          2669
      start                            72
      serial                            8
      virtio                            7
      services                      19504
      metrics                         491
      shutdown                        142
      exit                          13284
      ─────────────────────────────────────
      TOTAL                         36177
      ─────────────────────────────────────

    ========================================
      Result: ALL PHASES PASSED
      Total time: 36.1s
    ========================================

    ... (eval, grafana, bpf variants follow with similar output) ...

    ╔══════════════════════════════════════════════════════════════════════════════╗
    ║                         LIFECYCLE TEST SUMMARY                               ║
    ╠══════════════════════════════════════════════════════════════════════════════╣
    ║  Variant       Result    Build    Start    Serial   Virtio   Services  Exit  ║
    ╠══════════════════════════════════════════════════════════════════════════════╣
    ║  base          PASS      2669ms   72ms     8ms      7ms      19504ms   13284ms
    ║  eval          PASS      2588ms   63ms     7ms      7ms      26323ms   3805ms
    ║  grafana       PASS      2687ms   60ms     9ms      7ms      21594ms   4070ms
    ║  grafana-tap   PASS      6897ms   139ms    9ms      7ms      22630ms   3324ms
    ║  bpf           PASS      2617ms   65ms     7ms      8ms      21037ms   3582ms
    ╠══════════════════════════════════════════════════════════════════════════════╣
    ║  TOTAL: 5 passed, 0 failed                                                   ║
    ║  Total time: 3m 5s                                                           ║
    ╚══════════════════════════════════════════════════════════════════════════════╝

**Timing observations:**

- **SSH connection (19-26s)**: This is VM boot time - the test waits for SSH to
  become available, which requires QEMU to start, NixOS to boot (kernel, initrd,
  systemd), network to come up, and SSH daemon to accept connections.

- **Serial/virtio console checks (7-9ms)**: Very fast because they only verify
  the TCP socket is listening, not that boot is complete.

- **Base variant longer exit (13s vs 3-4s)**: The base variant has pmlogger
  enabled, which needs to flush archive buffers, write the final volume, and
  clean up metadata files before shutdown completes.

- **Grafana longer services**: Grafana takes additional time to initialize
  compared to other services.

- **TAP variant (grafana-tap)**: Uses direct network access (10.177.0.20) instead
  of port forwarding. Build time is slightly longer on first run due to different
  network configuration. Verifies BPF metrics (bpf.runq.latency, bpf.disk.all.latency)
  in addition to Grafana/Prometheus services.

**Variant-specific timeouts:**

Grafana variants have longer service timeouts (90s) as Grafana takes
additional time to initialize.

TAP Networking
^^^^^^^^^^^^^^

By default, MicroVMs use QEMU user-mode networking with port forwarding. For
direct network access (no port forwarding), use TAP networking:

::

    # Verify host environment
    nix run .#pcp-check-host

    # Create bridge, TAP device, and NAT rules (requires sudo)
    sudo nix run .#pcp-network-setup

    # Build and run TAP-enabled VM
    nix build .#pcp-microvm-eval-tap
    ./result/bin/microvm-run

    # VM is now accessible at 10.177.0.20
    ssh root@10.177.0.20
    pminfo -h 10.177.0.20 kernel.all.load

    # Cleanup when done
    nix run .#pcp-vm-stop
    sudo nix run .#pcp-network-teardown

TAP networking is useful for testing network-facing scenarios or when port
forwarding is insufficient.

Grafana MicroVM Example
^^^^^^^^^^^^^^^^^^^^^^^

The Grafana variant includes Prometheus and pre-configured dashboards. With TAP
networking, you can access the Grafana web interface directly::

    # 1. Verify host environment
    nix run .#pcp-check-host

    # 2. Setup TAP networking (requires sudo)
    sudo nix run .#pcp-network-setup

    # 3. Build and run Grafana VM
    nix build .#pcp-microvm-grafana-tap -o result-grafana
    ./result-grafana/bin/microvm-run &

    # 4. Access services (VM IP: 10.177.0.20)
    #    Grafana:    http://10.177.0.20:3000 (admin/admin)
    #    Prometheus: http://10.177.0.20:9090
    #    pmproxy:    http://10.177.0.20:44322
    #    SSH:        ssh root@10.177.0.20 (password: pcp)

    # 5. Cleanup
    nix run .#pcp-vm-stop
    sudo nix run .#pcp-network-teardown

Services in the MicroVM
^^^^^^^^^^^^^^^^^^^^^^^

The evaluation MicroVM runs several services for comprehensive PCP testing:

**PCP Services:**

- **pmcd** - Performance Metrics Collection Daemon (port 44321)
- **pmproxy** - REST API gateway (port 44322)
- **pmie-test** - Dedicated pmie instance running custom test rules
- **stress-ng-test** - Synthetic CPU workload for pmie testing

**Comparison Tools:**

- **node_exporter** - Prometheus metrics exporter (port 9100)
- **below** - Meta's time-traveling resource monitor

Verify services are running::

    # SSH into the VM
    nix run .#pcp-vm-ssh

    # Check PCP services
    systemctl status pmcd pmproxy pmie-test stress-ng-test

    # Query PCP metrics
    pminfo -f kernel.all.load

    # Compare with node_exporter
    curl -s localhost:9100/metrics | grep node_load

    # Use below for time-traveling analysis
    below live

pmie Testing
^^^^^^^^^^^^

The evaluation MicroVM includes automated pmie (Performance Metrics Inference
Engine) testing using a synthetic workload. This verifies that pmie can:

1. Monitor live metrics from pmcd
2. Evaluate rules against those metrics
3. Trigger actions when conditions are met

**Architecture:**

::

    ┌─────────────────────────────────────────────────────────────────┐
    │                      NixOS MicroVM                              │
    │                                                                 │
    │  ┌─────────────────┐    ┌─────────────────┐                     │
    │  │ stress-ng-test  │───▶│      pmcd       │◀───┐                │
    │  │   (systemd)     │    │  (metrics)      │    │                │
    │  │                 │    └─────────────────┘    │                │
    │  │ - 20s stress    │            │              │                │
    │  │ - 10s idle      │            ▼              │                │
    │  │ - loop forever  │    ┌─────────────────┐    │                │
    │  └─────────────────┘    │    pmie-test    │────┘                │
    │                         │                 │                     │
    │                         │ Rule: detect    │                     │
    │                         │ CPU elevation   │                     │
    │                         │                 │                     │
    │                         │ Action: log to  │                     │
    │                         │ alerts.log      │                     │
    │                         └─────────────────┘                     │
    └─────────────────────────────────────────────────────────────────┘

**pmie Test Rules:**

The ``pmie-test`` service monitors for CPU spikes caused by stress-ng:

- **cpu_elevated** - Detects when ``kernel.all.cpu.nice`` exceeds 10% of total
  CPU (stress-ng runs at Nice=19, so its CPU time goes to the ``nice`` metric)
- **heartbeat** - Touches a file every 5 seconds to confirm pmie is evaluating

**Verifying pmie Testing:**

::

    # SSH into the VM
    nix run .#pcp-vm-ssh

    # Check pmie alerts (generated during stress cycles)
    cat /var/log/pcp/pmie/alerts.log

    # Expected output during stress:
    # 2026-02-11T22:27:43+00:00 [ALERT] CPU elevated
    # 2026-02-11T22:27:48+00:00 [ALERT] CPU elevated

    # Check heartbeat is being updated
    ls -la /var/log/pcp/pmie/heartbeat

    # View pmie-test service status
    systemctl status pmie-test

**Key Implementation Details:**

- stress-ng runs at Nice=19 with CPUQuota=50% to avoid overwhelming the VM
- CPU time appears in ``kernel.all.cpu.nice`` (not ``user``) due to Nice level
- The threshold (10%) accounts for multi-CPU systems using ``hinv.ncpu``
- pmie evaluates rules every 5 seconds (``delta = 5 sec``)

Fedora vs NixOS Comparison
^^^^^^^^^^^^^^^^^^^^^^^^^^

The NixOS MicroVM deployment differs from traditional Fedora installations in
several key ways:

**Path Differences:**

.. list-table::
   :header-rows: 1
   :widths: 25 35 40

   * - Item
     - Fedora
     - NixOS MicroVM
   * - PCP config
     - ``/etc/pcp.conf``
     - ``/nix/store/<hash>-pcp/share/pcp/etc/pcp.conf``
   * - Binaries
     - ``/usr/bin/pminfo``
     - ``/nix/store/<hash>-pcp/bin/pminfo``
   * - Libraries
     - ``/usr/lib64/libpcp.so``
     - ``/nix/store/<hash>-pcp/lib/libpcp.so``
   * - PMDAs
     - ``/var/lib/pcp/pmdas/``
     - ``/nix/store/<hash>-pcp/var/lib/pcp/pmdas/``

**FHS Compatibility:**

To ease the transition for users familiar with Fedora/RHEL, the NixOS module
creates symlinks at standard FHS paths:

- ``/etc/pcp.conf`` → Nix store pcp.conf
- ``/etc/pcp.env`` → Nix store pcp.env (can be sourced)
- ``/etc/pcp/`` → Nix store config directory

This allows Fedora-style commands like ``source /etc/pcp.env`` to work.

**Service Differences:**

.. list-table::
   :header-rows: 1
   :widths: 25 35 40

   * - Aspect
     - Fedora
     - NixOS
   * - Service type
     - ``Type=notify``
     - ``Type=forking``
   * - Directory management
     - Manual or package scripts
     - systemd ``RuntimeDirectory``, ``StateDirectory``
   * - Security
     - Default
     - Hardening options (``NoNewPrivileges``, ``ProtectHome``, etc.)
   * - Environment
     - Read from ``/etc/pcp.conf``
     - Set via systemd ``Environment=``

**Patches Required for NixOS:**

1. **gnumakefile-nix-fixes.patch** - Build system fixes for Nix paths
2. **python-libpcp-nix.patch** - Python ctypes library loading via LD_LIBRARY_PATH
3. **python-pmapi-no-reconnect.patch** - Fix IPC table corruption with derived metrics
4. **shell-portable-pwd.patch** - Use portable pwd detection instead of ``/bin/pwd``

**Metric Parity:**

Both deployments provide equivalent functionality:

- ~2700 metrics available
- pmcd and pmproxy services running
- Python tools (``pcp dstat``, ``pmrep``) working correctly
- Derived metrics configured

OCI Container
-------------

The ``pcp-container`` output produces an OCI-compatible container image for
Docker or Podman deployment.

Building and Running
^^^^^^^^^^^^^^^^^^^^

::

    # Build the container image
    nix build .#pcp-container

    # Load into Docker
    docker load < result

    # Run pmcd in foreground
    docker run -d -p 44321:44321 -p 44322:44322 --name pcp pcp:latest

    # Query metrics from host
    pminfo -h localhost kernel.all.load

    # Interactive shell
    docker exec -it pcp /bin/bash

Container Security
^^^^^^^^^^^^^^^^^^

The container follows security best practices:

- **Non-root execution**: Runs as ``pcp`` user (UID 990), not root
- **Minimal image**: Contains only PCP and essential dependencies
- **Layered build**: Uses ``buildLayeredImage`` for efficient Docker layer caching
- **Explicit ports**: Only exposes pmcd (44321) and pmproxy (44322)

Container Structure
^^^^^^^^^^^^^^^^^^^

::

    /                           # Image root
    ├── bin/                    # PCP binaries
    ├── lib/                    # Libraries
    ├── var/lib/pcp/            # PCP state (owned by pcp:pcp)
    ├── var/log/pcp/            # Logs (owned by pcp:pcp)
    ├── run/pcp/                # Runtime (owned by pcp:pcp)
    └── etc/
        ├── passwd              # Contains pcp user
        └── group               # Contains pcp group

The container is configured with:

- ``PCP_CONF`` pointing to the bundled pcp.conf
- Working directory set to ``/var/lib/pcp``
- Default command: ``pmcd -f`` (foreground mode)

Container Testing
^^^^^^^^^^^^^^^^^

The container test verifies the full lifecycle::

    nix run .#pcp-container-test

**Test phases:**

1. Build image (``nix build .#pcp-container``)
2. Load into Docker/Podman
3. Start container with port mappings
4. Verify pmcd process running
5. Verify port 44321 listening
6. Verify kernel metrics (kernel.all.load, cpu.user, mem.physmem)
7. Verify BPF metrics (if available)
8. Graceful shutdown
9. Cleanup

**Quick test** (assumes image already loaded)::

    nix run .#pcp-container-test-quick

Kubernetes Testing
^^^^^^^^^^^^^^^^^^

The Kubernetes test deploys PCP as a privileged DaemonSet in minikube::

    # Start minikube (if not running)
    nix run .#pcp-minikube-start

    # Run the full test
    nix run .#pcp-k8s-test

**Test phases:**

1. Verify minikube is running
2. Build container image
3. Load image into minikube's Docker
4. Deploy DaemonSet to ``pcp-test`` namespace
5. Wait for pods to be ready (one per node)
6. Verify pmcd process in each pod
7. Verify ports 44321, 44322 listening
8. Verify kernel metrics from each node
9. Verify BPF metrics (if BTF available)
10. Cleanup namespace

**Quick test** (skip build, assumes image loaded)::

    nix run .#pcp-k8s-test-quick

**Minikube setup helper:**

The ``pcp-minikube-start`` app configures minikube with optimal settings
for PCP testing (4 CPUs, 8GB RAM, docker driver)::

    nix run .#pcp-minikube-start

Development Shell
-----------------

The development shell provides PCP build dependencies plus debugging tools::

    # Enter the shell
    nix develop

    # Available tools:
    # - All PCP build dependencies (autoconf, bison, flex, etc.)
    # - gdb for debugging
    # - valgrind for memory analysis (Linux only)
    # - lldb for debugging (macOS only)

    # Build from source manually
    ./configure --prefix=$PWD/install
    make -j$(nproc)
    make install

The shell displays the PCP icon on entry (via jp2a) and provides hints for
getting started.

Future Improvements
-------------------

Unbundle Vendored Dependencies
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

PCP vendors several libraries that already exist as packages in nixpkgs:

- **inih**: INI file parser (``pkgs.inih``)
- **hiredis/hiredis-cluster**: Redis clients (``pkgs.hiredis``)
- **jsonsl**: JSON streaming parser
- **libbpf/bpftool**: BPF tooling (``pkgs.libbpf``, complex due to kernel version coupling)
- **htop source**: for pcp-htop (modified fork, ``pkgs.htop``)
- **BCC libbpf-tools**: BPF CO-RE tools (``pkgs.bcc``)

Since nixpkgs already provides packages for most of these libraries, they could
be used directly instead of compiling the vendored copies. This would:

- **Improve build performance**: Skip recompiling libraries that are already
  built and cached in the Nix store
- **Reduce closure size**: Share libraries with other packages rather than
  bundling duplicates
- **Automatically propagate security updates**: When nixpkgs updates a library,
  PCP would automatically receive the fix
- **Follow nixpkgs best practices**: Vendoring is discouraged in favor of
  explicit dependencies

This requires upstream cooperation to ensure clean library boundaries and
configure flags to use system libraries.

PMDA Extensibility
^^^^^^^^^^^^^^^^^^

Consider using ``lib.makeSearchPath`` to allow users to add custom PMDAs
without rebuilding the package, via ``PCP_PMDA_PATH`` or similar mechanism.

NixOS Module Enhancements
^^^^^^^^^^^^^^^^^^^^^^^^^

The current NixOS module (``nix/nixos-module.nix``) provides basic functionality.
Future enhancements could include:

- Declarative PMDA configuration (enable/disable specific PMDAs)
- Log rotation and retention policies via systemd
- Integration with ``services.prometheus`` for metric export
- Custom archive retention settings for pmlogger

Integrate into Nixpkgs
^^^^^^^^^^^^^^^^^^^^^^

To fully integrate into the nixpkgs repository:

1. Submit the package to ``pkgs/by-name/pc/pcp/``
2. Add NixOS module to ``nixos/modules/services/monitoring/``
3. Add VM test to ``nixos/tests/all-tests.nix``
4. Reference via ``passthru.tests = { inherit (nixosTests) pcp; };``

This would enable ``nix-build -A nixosTests.pcp`` and ensure tests run
automatically in nixpkgs CI.

Summary
-------

The Nix packaging of PCP provides:

✅ Reproducible builds from source
✅ All core PMDAs and tools
✅ Pre-compiled BPF PMDA (CO-RE eBPF, fast startup, low memory)
✅ Python and Perl language bindings
✅ Systemd integration (services, tmpfiles, sysusers)
✅ Split outputs for minimal installations
✅ NixOS module (``services.pcp``) with pmcd, pmlogger, pmie, pmproxy
✅ MicroVMs for development and testing (user-mode and TAP networking)
✅ Lifecycle testing framework for fine-grained VM validation (7 phases)
✅ Serial console debugging (ttyS0 slow/early, hvc0 fast/virtio)
✅ pmie testing with synthetic workload verification
✅ OCI container for Docker/Podman deployment
✅ Development shell with debugging tools

Some features require enabling optional flags, and a few PMDAs need packages
not yet available in nixpkgs.

For questions or contributions, please open an issue on the `PCP GitHub
repository <https://github.com/performancecopilot/pcp>`_ or the `nixpkgs
repository <https://github.com/NixOS/nixpkgs>`_.
