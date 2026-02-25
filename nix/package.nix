# nix/package.nix
#
# PCP package derivation.
# Version is automatically derived from VERSION.pcp (the configure script's
# source of truth), eliminating manual maintenance when PCP is released.
#
{ pkgs }:
let
  lib = pkgs.lib;

  # ─── Version Parsing ───────────────────────────────────────────────────
  # Parse version from VERSION.pcp with explicit error handling.
  # VERSION.pcp format:
  #   PACKAGE_MAJOR=7
  #   PACKAGE_MINOR=1
  #   PACKAGE_REVISION=1
  #   PACKAGE_BUILD=1
  #
  versionFile = builtins.readFile ./../VERSION.pcp;

  parseField = field:
    let
      # Match pattern: field=digits (handles multi-line file)
      # The regex needs to work across the whole file content
      lines = lib.splitString "\n" versionFile;
      matchingLines = builtins.filter (line:
        builtins.match "^${field}=([0-9]+).*" line != null
      ) lines;
      matchedLine = if matchingLines == [] then null else builtins.head matchingLines;
      match = if matchedLine == null then null
              else builtins.match "^${field}=([0-9]+).*" matchedLine;
    in
      if match == null then
        throw ''
          Failed to parse ${field} from VERSION.pcp.
          Expected format: ${field}=<number>
          File contents:
          ${versionFile}
        ''
      else
        builtins.head match;

  major = parseField "PACKAGE_MAJOR";
  minor = parseField "PACKAGE_MINOR";
  revision = parseField "PACKAGE_REVISION";

  # Validate parsed version components
  version = let
    v = "${major}.${minor}.${revision}";
  in
    assert lib.assertMsg (major != "") "PACKAGE_MAJOR is empty";
    assert lib.assertMsg (minor != "") "PACKAGE_MINOR is empty";
    assert lib.assertMsg (revision != "") "PACKAGE_REVISION is empty";
    v;

  # ─── Feature Flags ─────────────────────────────────────────────────────
  # All enabled features are open source. Proprietary integrations are excluded
  # via configureFlags (mongodb, mysql, nutcracker) or postInstall (mssql).
  #
  withSystemd = pkgs.stdenv.isLinux;  # systemd service management
  withPfm = pkgs.stdenv.isLinux;      # hardware performance counters (libpfm)
  withBpf = pkgs.stdenv.isLinux;      # eBPF tracing (bcc, bpf, bpftrace PMDAs)
  withSnmp = true;                    # SNMP network monitoring
  withPythonHttp = true;              # Python HTTP client (requests)
  withPerlHttp = true;                # Perl HTTP client (LWP)

  # ─── Source Filtering ──────────────────────────────────────────────────
  # Use cleanSourceWith to exclude build outputs and non-essential files
  # from the Nix store. This prevents accidental inclusion of result symlinks,
  # test-results/, etc. which would cause unnecessary rebuilds.
  #
  # IMPORTANT: Only exclude top-level result* symlinks, not files like result.c
  # or result.o which are legitimate source/build files.
  #
  cleanedSrc = lib.cleanSourceWith {
    src = ./..;
    filter = path: type:
      let
        baseName = baseNameOf path;
        parentDir = dirOf path;
        isTopLevel = parentDir == toString ./..;
        # Patterns to exclude
        isExcluded =
          # Build output symlinks (only at top level, and only exact matches or result-*)
          (isTopLevel && (baseName == "result" || lib.hasPrefix "result-" baseName)) ||
          baseName == "test-results" ||
          # Editor/IDE artifacts
          baseName == ".vscode" ||
          baseName == ".idea" ||
          lib.hasSuffix ".swp" baseName ||
          lib.hasSuffix ".swo" baseName ||
          # Nix build artifacts
          baseName == ".direnv" ||
          # Baseline metrics (Phase 0)
          baseName == "baseline-metrics.txt" ||
          baseName == "baseline-check.txt" ||
          baseName == "phase1-metrics.txt";
      in
        !isExcluded;
  };

in
pkgs.stdenv.mkDerivation rec {
  pname = "pcp";
  inherit version;
  src = cleanedSrc;

  outputs = [
    "out"
    "man"
    "doc"
  ];

  nativeBuildInputs = with pkgs; [
    autoconf
    automake
    pkg-config
    bison
    flex
    which
    perl
    python3
    python3.pkgs.setuptools
    makeWrapper
    binutils
  ] ++ lib.optionals withBpf [
    llvmPackages.clang
    llvmPackages.llvm
  ];

  buildInputs = with pkgs; [
    zlib
    ncurses
    readline
    openssl
    libuv
    cyrus_sasl
    inih
    xz
    python3
    perl
    rrdtool
  ] ++ lib.optionals pkgs.stdenv.isLinux [
    avahi
    lvm2
  ] ++ lib.optionals withSystemd [
    systemd
  ] ++ lib.optionals withPfm [
    libpfm
  ] ++ lib.optionals withBpf [
    libbpf
    bcc
    elfutils
  ] ++ lib.optionals withSnmp [
    net-snmp
  ] ++ lib.optionals withPythonHttp [
    python3.pkgs.requests
  ] ++ lib.optionals withPerlHttp [
    perlPackages.JSON
    perlPackages.LWPUserAgent
  ];

  configureFlags = lib.concatLists [

    [
      "--prefix=${placeholder "out"}"
      "--sysconfdir=${placeholder "out"}/etc"
      "--localstatedir=${placeholder "out"}/var"
      "--with-rcdir=${placeholder "out"}/etc/init.d"
      "--with-tmpdir=/tmp"
      "--with-logdir=${placeholder "out"}/var/log/pcp"
      "--with-rundir=/run/pcp"
    ]

    [
      "--with-user=pcp"
      "--with-group=pcp"
    ]

    [
      "--with-make=make"
      "--with-tar=tar"
      "--with-python3=${lib.getExe pkgs.python3}"
    ]

    [
      "--with-perl=yes"
      "--with-threads=yes"
    ]

    [
      "--with-secure-sockets=yes"
      "--with-transparent-decompression=yes"
    ]

    (if pkgs.stdenv.isLinux then [ "--with-discovery=yes" ] else [ "--with-discovery=no" ])

    [
      "--with-dstat-symlink=no"
      "--with-pmdamongodb=no"
      "--with-pmdamysql=no"
      "--with-pmdanutcracker=no"
      "--with-qt=no"
      "--with-infiniband=no"
      "--with-selinux=no"
    ]

    (if withSystemd then [ "--with-systemd=yes" ] else [ "--with-systemd=no" ])
    (if withPfm then [ "--with-perfevent=yes" ] else [ "--with-perfevent=no" ])
    (
      if withBpf then
        [
          "--with-pmdabcc=yes"
          "--with-pmdabpf=yes"
          "--with-pmdabpftrace=yes"
        ]
      else
        [
          "--with-pmdabcc=no"
          "--with-pmdabpf=no"
          "--with-pmdabpftrace=no"
        ]
    )

    (if pkgs.stdenv.isLinux then [ "--with-devmapper=yes" ] else [ "--with-devmapper=no" ])

    (if withSnmp then [ "--with-pmdasnmp=yes" ] else [ "--with-pmdasnmp=no" ])
  ];


  patches = [
    ./patches/gnumakefile-nix-fixes.patch
    ./patches/python-libpcp-nix.patch
    # Fix Python wrapper bug: pmReconnectContext() after pmRegisterDerived() causes
    # IPC table corruption when registering multiple derived metrics. The reconnect
    # calls __dmclosecontext()/__dmopencontext() which corrupts the IPC version table,
    # causing subsequent pmGetPMNSLocation() to fail with PM_ERR_NOPMNS.
    # Symptoms: "pcp dstat" fails after first few metrics with "PMNS not accessible"
    ./patches/python-pmapi-no-reconnect.patch
    # Use portable pwd fallback instead of hardcoded /bin/pwd which doesn't exist on NixOS
    ./patches/shell-portable-pwd.patch
  ];

  postPatch = ''
    # Fix shebangs (can't be done as static patch - needs Nix store paths)
    patchShebangs src build configure scripts man

    # Fix build scripts that use /var/tmp (not available in Nix sandbox)
    # Substitute with TMPDIR which Nix sets up correctly
    for f in src/pmdas/bind2/mk.rewrite \
             src/pmdas/jbd2/mk.rewrite \
             src/pmdas/linux/mk.rewrite \
             src/pmdas/linux_proc/mk.rewrite; do
      if [ -f "$f" ]; then
        substituteInPlace "$f" --replace '/var/tmp' "$TMPDIR"
      fi
    done
  '';

  preConfigure = ''
    # Ensure AR is in PATH and exported for configure script
    export AR="${pkgs.stdenv.cc.bintools.bintools}/bin/ar"
  '';

  hardeningDisable = lib.optionals withBpf [ "zerocallusedregs" ];

  BPF_CFLAGS = lib.optionalString withBpf "-fno-stack-protector -Wno-error=unused-command-line-argument";
  CLANG = lib.optionalString withBpf (lib.getExe pkgs.llvmPackages.clang);

  # Set AR explicitly so configure can find it (configure checks $AR first)
  AR = "${pkgs.stdenv.cc.bintools.bintools}/bin/ar";

  SYSTEMD_SYSTEMUNITDIR = lib.optionalString withSystemd "${placeholder "out"}/lib/systemd/system";
  SYSTEMD_TMPFILESDIR = lib.optionalString withSystemd "${placeholder "out"}/lib/tmpfiles.d";
  SYSTEMD_SYSUSERSDIR = lib.optionalString withSystemd "${placeholder "out"}/lib/sysusers.d";

  postInstall = ''
    # Build the combined PMNS root file
    # The individual root_* files exist but pmcd needs a combined 'root' file
    # Use pmnsmerge to combine all the root_* files into one
    (
      cd $out/var/lib/pcp/pmns
      export PCP_DIR=$out
      export PCP_CONF=$out/etc/pcp.conf
      . $out/etc/pcp.env

      # Merge all the root_* files into the combined root file
      # Order matters: root_root first (base), then others
      $out/libexec/pcp/bin/pmnsmerge -a \
        $out/libexec/pcp/pmns/root_root \
        $out/libexec/pcp/pmns/root_pmcd \
        $out/libexec/pcp/pmns/root_linux \
        $out/libexec/pcp/pmns/root_proc \
        $out/libexec/pcp/pmns/root_xfs \
        $out/libexec/pcp/pmns/root_jbd2 \
        $out/libexec/pcp/pmns/root_kvm \
        $out/libexec/pcp/pmns/root_mmv \
        $out/libexec/pcp/pmns/root_bpf \
        $out/libexec/pcp/pmns/root_pmproxy \
        root
    )

    # Remove runtime state directories
    rm -rf $out/var/{run,log} $out/var/lib/pcp/tmp || true

    # Remove derived metric configs for proprietary software
    # - mssql.conf: Microsoft SQL Server (proprietary), also has novalue() syntax errors
    rm -f $out/var/lib/pcp/config/derived/mssql.conf || true

    # Move vendor config to share
    if [ -d "$out/etc" ]; then
      mkdir -p $out/share/pcp/etc
      mv $out/etc/* $out/share/pcp/etc/
      rmdir $out/etc || true

      # Fix paths in pcp.conf to point to new locations
      substituteInPlace $out/share/pcp/etc/pcp.conf \
        --replace-quiet "$out/etc/pcp" "$out/share/pcp/etc/pcp" \
        --replace-quiet "$out/etc/sysconfig" "$out/share/pcp/etc/sysconfig" \
        --replace-quiet "PCP_ETC_DIR=$out/etc" "PCP_ETC_DIR=$out/share/pcp/etc"

      # Fix symlinks that pointed to /etc/pcp/...
      find $out/var/lib/pcp -type l | while read link; do
        target=$(readlink "$link")
        if [[ "$target" == *"/etc/pcp/"* ]]; then
          suffix="''${target#*/etc/pcp/}"
          rm "$link"
          ln -sf "$out/share/pcp/etc/pcp/$suffix" "$link"
        fi
      done
    fi

    # Fix broken symlinks with double /nix/store prefix
    # These occur when the build system prepends a path to an already-absolute path
    for broken_link in "$out/share/pcp/etc/pcp/pm"{search/pmsearch,series/pmseries}.conf; do
      [[ -L "$broken_link" ]] && rm "$broken_link" && \
        ln -sf "$out/share/pcp/etc/pcp/pmproxy/pmproxy.conf" "$broken_link"
    done

    # Fix pmcd/rc.local symlink (points to libexec/pcp/services/local)
    if [[ -L "$out/share/pcp/etc/pcp/pmcd/rc.local" ]]; then
      rm "$out/share/pcp/etc/pcp/pmcd/rc.local"
      ln -sf "$out/libexec/pcp/services/local" "$out/share/pcp/etc/pcp/pmcd/rc.local"
    fi

    # Fix proc.conf novalue() syntax errors (upstream bug):
    # 1. Remove spaces after commas in novalue() parameters
    # 2. Remove invalid 'indom=' parameter (novalue() only accepts type, semantics, units)
    # Must run after /etc is moved to share/pcp/etc
    for procconf in $out/share/pcp/etc/pcp/derived/proc.conf $out/var/lib/pcp/config/derived/proc.conf; do
      if [ -f "$procconf" ]; then
        sed -i -e 's/novalue(type=u64, semantics=counter, units=Kbyte, indom=157\.2)/novalue(type=u64,semantics=counter,units=Kbyte)/g' \
               -e 's/novalue(type=u64,semantics=counter,units=Kbyte,indom=157\.2)/novalue(type=u64,semantics=counter,units=Kbyte)/g' "$procconf"
      fi
    done

    # Create .py symlinks for Python PMDA utility files
    # PCP's Python PMDA framework expects pmdautil.py but ships pmdautil.python
    # On NixOS, the /nix/store is read-only, so we create symlinks at build time
    # instead of letting the runtime code fail trying to create them
    for pmda_dir in $out/libexec/pcp/pmdas/*/; do
      for pyfile in "$pmda_dir"*.python; do
        if [ -f "$pyfile" ]; then
          base=$(basename "$pyfile" .python)
          pylink="$pmda_dir$base.py"
          if [ ! -e "$pylink" ]; then
            ln -s "$(basename "$pyfile")" "$pylink"
          fi
        fi
      done
    done

    # Also create symlinks for all Python files recursively in libexec/pcp/pmdas
    # This covers BCC modules in modules/ subdirectory which pmdabcc.python
    # tries to load with .py extension. On NixOS /nix/store is read-only,
    # so we create all .py symlinks at build time.
    find $out/libexec/pcp/pmdas -name "*.python" -type f 2>/dev/null | while read pyfile; do
      base=$(basename "$pyfile" .python)
      pylink="$(dirname "$pyfile")/$base.py"
      if [ ! -e "$pylink" ]; then
        ln -s "$(basename "$pyfile")" "$pylink"
      fi
    done

    # Also create .py symlinks for the symlinks in var/lib/pcp/pmdas/
    # pmdabcc.python walks PCP_PMDASADM_DIR (var/lib/pcp/pmdas/bcc) and looks
    # for .py files, but that directory contains symlinks to libexec.
    # We create .py -> .python symlinks here so pmdabcc can find them.
    find $out/var/lib/pcp/pmdas -name "*.python" -type l 2>/dev/null | while read pyfile; do
      base=$(basename "$pyfile" .python)
      pylink="$(dirname "$pyfile")/$base.py"
      if [ ! -e "$pylink" ]; then
        ln -s "$(basename "$pyfile")" "$pylink"
      fi
    done

    # Move man pages to $man output
    if [ -d "$out/share/man" ]; then
      mkdir -p $man/share
      mv $out/share/man $man/share/
    fi

    # Move documentation to $doc output
    for docdir in $out/share/doc/pcp*; do
      if [ -d "$docdir" ]; then
        mkdir -p $doc/share/doc
        mv "$docdir" $doc/share/doc/
      fi
    done
  '';

  # Wrap Python scripts with correct environment for NixOS
  #
  # Each environment variable solves a specific NixOS compatibility issue:
  #
  # PCP_DIR: libpcp's config.c searches for pcp.conf in this order:
  #   1. $PCP_CONF (if set)
  #   2. $PCP_DIR/etc/pcp.conf (if $PCP_DIR set)
  #   3. /etc/pcp.conf (fallback)
  # On NixOS, pcp.conf lives in the store, so we must set PCP_DIR.
  # This enables pmGetConfig() to return correct paths like PCP_SYSCONF_DIR,
  # which pcp-dstat needs to find its derived metric configs at $PCP_SYSCONF_DIR/dstat.
  #
  # PYTHONPATH: Standard path for PCP's Python modules in site-packages.
  #
  # LD_LIBRARY_PATH: Python's ctypes.util.find_library() uses ldconfig to
  # locate shared libraries. On NixOS, ldconfig doesn't know about /nix/store
  # paths, so find_library("pcp") returns None. Our python-libpcp-nix.patch
  # adds a fallback that searches LD_LIBRARY_PATH when find_library fails.
  # Without this, CDLL(None) loads the Python executable itself, causing
  # "undefined symbol: pmGetChildren" errors.
  #
  postFixup = ''
    # Wrap Python scripts in libexec/pcp/bin
    for script in $out/libexec/pcp/bin/pcp-*; do
      if head -1 "$script" 2>/dev/null | grep -q python; then
        wrapProgram "$script" \
          --set PCP_DIR "$out/share/pcp" \
          --prefix PYTHONPATH : "$out/lib/${pkgs.python3.libPrefix}/site-packages" \
          --prefix LD_LIBRARY_PATH : "$out/lib"
      fi
    done

    # Wrap pmpython binary - it executes Python but doesn't set PYTHONPATH
    # This is needed for pmrep, pmiostat, and other tools that use #!/usr/bin/env pmpython
    if [ -f "$out/bin/pmpython" ]; then
      wrapProgram "$out/bin/pmpython" \
        --set PCP_DIR "$out/share/pcp" \
        --prefix PYTHONPATH : "$out/lib/${pkgs.python3.libPrefix}/site-packages" \
        --prefix LD_LIBRARY_PATH : "$out/lib"
    fi

    # Also wrap the main pcp command (Python-based dispatcher)
    if [ -f "$out/bin/pcp" ]; then
      wrapProgram "$out/bin/pcp" \
        --set PCP_DIR "$out/share/pcp" \
        --prefix PYTHONPATH : "$out/lib/${pkgs.python3.libPrefix}/site-packages" \
        --prefix LD_LIBRARY_PATH : "$out/lib"
    fi
  '';

  doCheck = false;
  enableParallelBuilding = true;

  meta = with lib; {
    description = "Performance Co-Pilot - system performance monitoring toolkit";
    homepage = "https://pcp.io";
    license = licenses.gpl2Plus;
    platforms = platforms.linux ++ platforms.darwin;
    mainProgram = "pminfo";
  };
}
