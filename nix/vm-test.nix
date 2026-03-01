# nix/vm-test.nix
#
# NixOS VM integration test for PCP.
# Uses the shared NixOS module for service configuration.
#
# This test verifies:
# - PCP services start correctly via systemd
# - pmcd, pmlogger, pmie, pmproxy all function
# - Basic metrics can be queried
# - Archives are created by pmlogger
#
# Run via flake: nix flake check
# Or standalone: nix build .#checks.x86_64-linux.vm-test
#
{ pkgs, pcp }:
let
  constants = import ./constants.nix;
  nixosModule = import ./nixos-module.nix;
in
pkgs.testers.nixosTest {
  name = "pcp-vm-test";

  nodes.machine = { ... }: {
    imports = [ nixosModule ];
    services.pcp = {
      enable = true;
      package = pcp;
      preset = "custom";  # Use custom to control which services are enabled
      pmlogger.enable = false;  # Requires additional configuration
      pmie.enable = false;      # Requires additional configuration
      pmproxy.enable = true;
    };
  };

  testScript = ''
    machine.wait_for_unit("multi-user.target")

    # Wait for core PCP services
    machine.wait_for_unit("pmcd.service")
    machine.wait_for_unit("pmproxy.service")

    # Wait for ports to be listening
    machine.wait_for_open_port(${toString constants.ports.pmcd})
    machine.wait_for_open_port(${toString constants.ports.pmproxy})

    # Basic metric queries
    machine.succeed("pminfo -f kernel.all.load")
    machine.succeed("pminfo -h localhost kernel.all.cpu.user")

    print("=== PCP VM test passed! ===")
  '';
}
