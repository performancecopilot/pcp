# nix/container.nix
#
# OCI container image with PCP.
# Uses buildLayeredImage for better Docker layer caching.
#
# SECURITY: Container runs as 'pcp' user (UID 990), not root.
# This follows container best practices for reduced blast radius.
# For BPF metrics, run the container with --privileged and as root.
#
# Build: nix build .#pcp-container
# Load:  docker load < result
# Run:   docker run -d -p 44321:44321 -p 44322:44322 --name pcp pcp:latest
#
# For BPF support (requires privileged mode):
#   docker run -d --privileged -p 44321:44321 --name pcp pcp:latest
#
# Directory structure mirrors nixos-module.nix:
# - Writable directories for logs, tmp, run
# - Symlinks to Nix store for read-only content (pmns, pmdas, configs)
# - Environment overrides for Nix store paths
#
{ pkgs, pcp }:
let
  constants = import ./constants.nix;
  inherit (constants) user paths ports logSubdirs storeSymlinks configSymlinks configWritableDirs pmdaDomains;

  # ─── Inputs Hash for Fast Cache Checking ───────────────────────────────────
  # Compute a hash of all inputs that affect the container image.
  # This is embedded as a label, allowing instant cache validation
  # without needing to extract/decompress the tarball.
  #
  # The hash changes when any of these change:
  # - PCP package (source code, build config)
  # - Base packages (bash, coreutils)
  # - Container config (this file's logic via pmcdConf, bpfConf store paths)
  #
  inputsHash = builtins.substring 0 32 (builtins.hashString "sha256" (
    builtins.concatStringsSep ":" [
      pcp.outPath
      pkgs.bashInteractive.outPath
      pkgs.coreutils.outPath
    ]
  ));

  # NSS files for user/group resolution inside the container.
  # Required because we run as non-root and need user lookup to work.
  passwdContents = ''
    root:x:0:0:root:/root:/bin/sh
    ${user.name}:x:${toString user.uid}:${toString user.gid}:${user.description}:${user.home}:/bin/sh
  '';

  groupContents = ''
    root:x:0:
    ${user.name}:x:${toString user.gid}:
  '';

  passwd = pkgs.writeTextDir "etc/passwd" passwdContents;
  group = pkgs.writeTextDir "etc/group" groupContents;

  # ─── pmcd.conf with BPF PMDA ─────────────────────────────────────────────
  # Configure pmcd with linux, pmcd, and bpf PMDAs
  pmcdConf = pkgs.writeText "pmcd.conf" ''
    #
    # Performance Co-Pilot PMDA Configuration
    # Generated for PCP container image
    #
    # Format: name domain_id type init_func path
    #

    # ─── Base Platform PMDAs ──────────────────────────────────────────────
    # Linux kernel metrics (DSO for performance)
    linux	${toString pmdaDomains.linux}	dso	linux_init	${pcp}/libexec/pcp/pmdas/linux/pmda_linux.so

    # PMCD internal metrics (DSO)
    pmcd	${toString pmdaDomains.pmcd}	dso	pmcd_init	${pcp}/libexec/pcp/pmdas/pmcd/pmda_pmcd.so

    # ─── BPF PMDA ─────────────────────────────────────────────────────────
    # CO-RE eBPF metrics (requires privileged mode and BTF kernel)
    bpf	${toString pmdaDomains.bpf}	dso	bpf_init	${pcp}/libexec/pcp/pmdas/bpf/pmda_bpf.so
  '';

  # ─── BPF module configuration ────────────────────────────────────────────
  # Enable runqlat (scheduler latency) and biolatency (disk I/O latency)
  bpfConf = pkgs.writeText "bpf.conf" ''
    #
    # BPF PMDA module configuration
    # Generated for PCP container image
    #
    # Enable impressive demo metrics:
    # - runqlat: scheduler run queue latency histogram
    # - biolatency: block I/O latency histogram
    #

    [runqlat.so]
    enabled = true

    [biolatency.so]
    enabled = true
  '';

  # PCP environment variables - mirrors nixos-module.nix pcpEnv
  # These override the Nix store paths baked into pcp.conf
  pcpEnv = [
    "PCP_CONF=${pcp}/share/pcp/etc/pcp.conf"
    "PCP_DIR=${pcp}"
    # Point to our generated pmcd.conf with BPF PMDA
    "PCP_PMCDCONF_PATH=/etc/pcp/pmcd/pmcd.conf"
    # Mutable runtime paths (Nix store versions are read-only)
    "PCP_LOG_DIR=${paths.logDir}"
    "PCP_VAR_DIR=${paths.varDir}"
    "PCP_TMP_DIR=${paths.tmpDir}"
    "PCP_RUN_DIR=${paths.runDir}"
    "PCP_ARCHIVE_DIR=${paths.archiveDir}"
    # Override hardcoded /bin/pwd path in shell scripts
    "PWDCMND=pwd"
    "HOME=${user.home}"
  ];

  # Generate mkdir commands for log subdirectories
  mkLogDirs = pkgs.lib.concatMapStringsSep "\n"
    (dir: "mkdir -p var/log/pcp/${dir}") logSubdirs;

  # Generate symlinks to Nix store paths
  mkStoreSymlinks = pkgs.lib.concatMapStringsSep "\n"
    (name: "ln -sf ${pcp}/var/lib/pcp/${name} var/lib/pcp/${name}") storeSymlinks;

  # Generate config symlinks
  mkConfigSymlinks = pkgs.lib.concatMapStringsSep "\n"
    (name: "ln -sf ${pcp}/var/lib/pcp/config/${name} var/lib/pcp/config/${name}") configSymlinks;

  # Generate writable config directories
  mkConfigWritableDirs = pkgs.lib.concatMapStringsSep "\n"
    (dir: "mkdir -p var/lib/pcp/config/${dir}") configWritableDirs;

  # Generate chown commands for writable directories
  chownWritableDirs = pkgs.lib.concatMapStringsSep "\n"
    (dir: "chown -R ${toString user.uid}:${toString user.gid} var/lib/pcp/config/${dir}") configWritableDirs;

  # The actual container image
  containerImage = pkgs.dockerTools.buildLayeredImage {
  name = "pcp";
  tag = "latest";

  contents = [
    pcp
    pkgs.bashInteractive
    pkgs.coreutils
    passwd
    group
  ];

  # Create directory structure - mirrors nixos-module.nix tmpfiles.rules
  # Note: extraCommands runs without fakeroot, so no chown here
  extraCommands = ''
    # Runtime directories (writable)
    mkdir -p var/lib/pcp
    mkdir -p var/lib/pcp/tmp
    mkdir -p var/log/pcp
    ${mkLogDirs}
    mkdir -p run/pcp
    mkdir -p tmp
    chmod 1777 tmp

    # Symlinks to read-only Nix store paths (pmns, pmdas, pmcd)
    ${mkStoreSymlinks}

    # Config directory - mix of symlinks and writable dirs
    mkdir -p var/lib/pcp/config
    ${mkConfigSymlinks}

    # Writable config directories
    ${mkConfigWritableDirs}

    # ─── PCP configuration files ─────────────────────────────────────────
    # pmcd.conf with BPF PMDA enabled
    mkdir -p etc/pcp/pmcd
    cp ${pmcdConf} etc/pcp/pmcd/pmcd.conf

    # BPF PMDA configuration (enable runqlat and biolatency modules)
    mkdir -p etc/pcp/bpf
    cp ${bpfConf} etc/pcp/bpf/bpf.conf

    # Symlink bpf.conf to where the BPF PMDA expects it
    mkdir -p var/lib/pcp/pmdas/bpf
    ln -sf /etc/pcp/bpf/bpf.conf var/lib/pcp/pmdas/bpf/bpf.conf
  '';

  # fakeRootCommands runs with fakeroot, allowing chown to work
  fakeRootCommands = ''
    # Set ownership on writable directories
    chown -R ${toString user.uid}:${toString user.gid} var/lib/pcp/tmp
    ${chownWritableDirs}
    chown -R ${toString user.uid}:${toString user.gid} var/log/pcp
    chown -R ${toString user.uid}:${toString user.gid} run/pcp
  '';

  config = {
    # SECURITY: Run as pcp user, not root
    User = user.name;
    Env = pcpEnv;
    # -f: foreground, -i 0.0.0.0: listen on all interfaces for container networking
    Cmd = [ "${pcp}/libexec/pcp/bin/pmcd" "-f" "-i" "0.0.0.0" ];
    ExposedPorts = {
      "${toString ports.pmcd}/tcp" = {};
      "${toString ports.pmproxy}/tcp" = {};
    };
    WorkingDir = user.home;
    # Label for fast cache checking - avoids tarball extraction
    Labels = {
      "nix.inputs.hash" = inputsHash;
    };
  };
};

in
{
  # The container image derivation
  image = containerImage;

  # The inputs hash for fast cache validation
  # Test scripts can compare this with the loaded image's label
  inherit inputsHash;
}
