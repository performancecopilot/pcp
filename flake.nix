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
        };

        checks = lib.optionalAttrs pkgs.stdenv.isLinux {
          vm-test = import ./nix/vm-test.nix {
            inherit pkgs pcp;
          };
        };

        # Import modular development shell
        devShells.default = import ./nix/shell.nix { inherit pkgs pcp; };
      }
    );
}
