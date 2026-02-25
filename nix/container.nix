# nix/container.nix
#
# OCI container image with PCP.
# Uses buildLayeredImage for better Docker layer caching.
#
# SECURITY: Container runs as 'pcp' user (UID 990), not root.
# This follows container best practices for reduced blast radius.
#
# Build: nix build .#pcp-container
# Load:  docker load < result
# Run:   docker run -d -p 44321:44321 -p 44322:44322 --name pcp pcp:latest
#
# Directory structure mirrors nixos-module.nix:
# - Writable directories for logs, tmp, run
# - Symlinks to Nix store for read-only content (pmns, pmdas, configs)
# - Environment overrides for Nix store paths
#
{ pkgs, pcp }:
let
  constants = import ./constants.nix;
  inherit (constants) user paths ports logSubdirs storeSymlinks configSymlinks configWritableDirs;

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

  # PCP environment variables - mirrors nixos-module.nix pcpEnv
  # These override the Nix store paths baked into pcp.conf
  pcpEnv = [
    "PCP_CONF=${pcp}/share/pcp/etc/pcp.conf"
    "PCP_DIR=${pcp}"
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

in
pkgs.dockerTools.buildLayeredImage {
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
  };
}
