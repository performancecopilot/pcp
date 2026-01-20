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
- Technical details of Nix-specific patches
- Known limitations and future improvements

The Nix packaging work was initiated to bring PCP into the `nixpkgs
<https://github.com/NixOS/nixpkgs>`_ repository, enabling NixOS users to easily
install and run PCP.

Quick Start
-----------

Building with Flakes (Recommended)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

From the PCP repository root::

    # Build PCP
    nix build

    # Run pminfo
    nix run .#pcp -- pminfo --version

    # Enter a development shell
    nix develop

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
- **BPF/BCC PMDAs**: kernel tracing via eBPF
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

This test configuration serves as a reference for how to deploy PCP on NixOS
until a proper NixOS module is created.

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

NixOS Module
^^^^^^^^^^^^

A NixOS module (``services.pcp``) would provide:

- Systemd service management for ``pmcd``, ``pmlogger``, ``pmproxy``
- Declarative PMDA configuration
- Proper user/group creation
- Log rotation and retention policies
- Integration with ``services.prometheus`` for exporters

Integrate NixOS Test
^^^^^^^^^^^^^^^^^^^^

The VM test currently exists as a standalone file. To fully integrate into
nixpkgs:

1. Add to ``nixos/tests/all-tests.nix``
2. Reference via ``passthru.tests = { inherit (nixosTests) pcp; };`` in the
   package

This would allow running ``nix-build -A nixosTests.pcp`` and ensure the test
runs automatically in nixpkgs CI.

Summary
-------

The Nix packaging of PCP provides:

✅ Reproducible builds from source
✅ All core PMDAs and tools
✅ BPF/BCC kernel tracing support
✅ Python and Perl language bindings
✅ Systemd integration
✅ Split outputs for minimal installations

Some features require enabling optional flags, and a few PMDAs need packages
not yet available in nixpkgs.

For questions or contributions, please open an issue on the `PCP GitHub
repository <https://github.com/performancecopilot/pcp>`_ or the `nixpkgs
repository <https://github.com/NixOS/nixpkgs>`_.
