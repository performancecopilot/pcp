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

        devShells.default = pkgs.mkShell {
          inputsFrom = [ pcp ];
          packages = with pkgs; [
            gdb
            jp2a
          ] ++ lib.optionals pkgs.stdenv.isLinux [
            valgrind
          ] ++ lib.optionals pkgs.stdenv.isDarwin [
            lldb
          ];

          shellHook = ''
            # Display PCP logo on shell entry
            if [[ -f ./images/pcpicon-light.png ]]; then
              jp2a --colors ./images/pcpicon-light.png 2>/dev/null || true
            fi
            echo "PCP Development Shell"
            echo "Run './configure --help' to see build options"
            echo "Otherwise use 'nix build' to build the package"
          '';
        };
      }
    );
}
