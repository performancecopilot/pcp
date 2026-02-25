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
{ pkgs, pcp }:
let
  constants = import ./constants.nix;

  # NSS files for user/group resolution inside the container.
  # Required because we run as non-root and need user lookup to work.
  passwdContents = ''
    root:x:0:0:root:/root:/bin/sh
    pcp:x:990:990:Performance Co-Pilot:/var/lib/pcp:/bin/sh
  '';

  groupContents = ''
    root:x:0:
    pcp:x:990:
  '';

  passwd = pkgs.writeTextDir "etc/passwd" passwdContents;
  group = pkgs.writeTextDir "etc/group" groupContents;
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

  # Create required directories with pcp ownership (UID 990)
  extraCommands = ''
    mkdir -p var/lib/pcp var/log/pcp run/pcp tmp
    mkdir -p var/log/pcp/{pmcd,pmlogger,pmie,pmproxy}
    chown -R 990:990 var/lib/pcp var/log/pcp run/pcp
    chmod 1777 tmp
  '';

  config = {
    # SECURITY: Run as pcp user, not root
    User = "pcp";
    Env = [
      "PCP_CONF=${pcp}/share/pcp/etc/pcp.conf"
      "HOME=/var/lib/pcp"
    ];
    Cmd = [ "${pcp}/libexec/pcp/bin/pmcd" "-f" ];
    ExposedPorts = {
      "${toString constants.ports.pmcd}/tcp" = {};
      "${toString constants.ports.pmproxy}/tcp" = {};
    };
    WorkingDir = "/var/lib/pcp";
  };
}
