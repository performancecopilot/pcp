#
# flake.nix - PCP Nix packaging
#
# See also: ./docs/HowTos/nix/index.rst
{
  description = "Performance Co-Pilot (PCP) - system performance monitoring toolkit";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs =
    {
      self,
      nixpkgs,
      flake-utils,
    }:
    flake-utils.lib.eachDefaultSystem (
      system:
      let
        pkgs = nixpkgs.legacyPackages.${system};
        lib = pkgs.lib;

        # Import modular package definition
        pcp = import ./nix/package.nix { inherit pkgs; };

      in
      {
        packages = {
          default = pcp;
          inherit pcp;
        } // lib.optionalAttrs pkgs.stdenv.isLinux {
          # OCI container image (Linux only)
          pcp-container = import ./nix/container.nix { inherit pkgs pcp; };
        };

        checks = lib.optionalAttrs pkgs.stdenv.isLinux {
          vm-test = import ./nix/vm-test.nix {
            inherit pkgs pcp;
          };
        };

        # Import modular development shell
        devShells.default = import ./nix/shell.nix { inherit pkgs pcp; };

        # ─── Apps (Linux only) ─────────────────────────────────────────────
        apps = lib.optionalAttrs pkgs.stdenv.isLinux (
          let
            networkScripts = import ./nix/network-setup.nix { inherit pkgs; };
            vmScripts = import ./nix/microvm-scripts.nix { inherit pkgs; };
          in {
            # Network management
            pcp-check-host = {
              type = "app";
              program = "${networkScripts.check}/bin/pcp-check-host";
            };
            pcp-network-setup = {
              type = "app";
              program = "${networkScripts.setup}/bin/pcp-network-setup";
            };
            pcp-network-teardown = {
              type = "app";
              program = "${networkScripts.teardown}/bin/pcp-network-teardown";
            };
            # VM management
            pcp-vm-check = {
              type = "app";
              program = "${vmScripts.check}/bin/pcp-vm-check";
            };
            pcp-vm-stop = {
              type = "app";
              program = "${vmScripts.stop}/bin/pcp-vm-stop";
            };
            pcp-vm-ssh = {
              type = "app";
              program = "${vmScripts.ssh}/bin/pcp-vm-ssh";
            };
          }
        );
      }
    );
}
