# NixOS VM test for Performance Co-Pilot (PCP)
#
# This test verifies:
# - PCP package builds and installs correctly
# - pmcd daemon can start and listen on port 44321
# - Basic metrics can be queried via pminfo
#
# Run via flake: nix flake check
# Or standalone: nix build .#checks.x86_64-linux.vm-test
{
  pkgs,
  pcp,
}:

pkgs.testers.nixosTest {
  name = "pcp-vm-test";

  nodes.machine =
    { pkgs, ... }:
    {
      environment.systemPackages = [ pcp ];

      # Create pcp user/group required by pmcd
      users.users.pcp = {
        isSystemUser = true;
        group = "pcp";
        description = "Performance Co-Pilot daemon user";
      };
      users.groups.pcp = { };

      # Create required runtime directories
      systemd.tmpfiles.rules = [
        "d /var/lib/pcp 0755 pcp pcp -"
        "d /var/log/pcp 0755 pcp pcp -"
        "d /run/pcp 0755 pcp pcp -"
      ];
    };

  testScript = ''
    machine.wait_for_unit("multi-user.target")

    # Verify the package is installed and pminfo works
    machine.succeed("pminfo --version")

    # Find the actual pcp package path by resolving pminfo symlink
    # pminfo is at /nix/store/xxx-pcp-7.0.5/bin/pminfo
    # We need to get the package root to find libexec/pcp/bin/pmcd
    pminfo_real = machine.succeed("realpath $(which pminfo)").strip()
    print(f"pminfo real path: {pminfo_real}")

    # Get package root (two levels up from bin/pminfo)
    pkg_root = machine.succeed(f"dirname $(dirname {pminfo_real})").strip()
    pmcd_path = f"{pkg_root}/libexec/pcp/bin/pmcd"
    print(f"pmcd expected at: {pmcd_path}")

    # Verify pmcd exists
    machine.succeed(f"test -x {pmcd_path}")

    # PCP_CONF points to the main configuration file
    # After our postInstall, config is at share/pcp/etc/pcp.conf
    pcp_conf = f"{pkg_root}/share/pcp/etc/pcp.conf"

    # Start pmcd daemon in background with PCP_CONF set
    machine.succeed(f"PCP_CONF={pcp_conf} setsid {pmcd_path} -f > /tmp/pmcd.log 2>&1 &")

    # Give pmcd a moment to start or fail
    import time
    time.sleep(3)

    # Debug: check if pmcd is running and show log
    print("=== Checking pmcd process ===")
    ps_out = machine.succeed("ps aux | grep pmcd || true")
    print(ps_out)

    print("=== pmcd log contents ===")
    log_out = machine.succeed("cat /tmp/pmcd.log 2>/dev/null || echo 'No log file'")
    print(log_out)

    print("=== Checking listening ports ===")
    ports_out = machine.succeed("ss -tlnp | grep -E '44321|pmcd' || echo 'No pmcd ports found'")
    print(ports_out)

    # Wait for pmcd to start listening on its default port
    machine.wait_for_open_port(44321, timeout=30)

    # Query basic kernel metrics to verify pmcd is working
    machine.succeed(f"PCP_CONF={pcp_conf} pminfo -f kernel.all.load")

    # Additional verification: check pmcd is responding
    machine.succeed(f"PCP_CONF={pcp_conf} pminfo -h localhost kernel.all.cpu.user")

    print("=== PCP VM test passed! ===")
  '';
}

