#
# flake.nix - PCP Nix packaging
#
# Quick Start:
#   nix build                         # Build PCP package
#   nix develop                       # Development shell
#   nix flake show                    # List all outputs
#
# Run All Tests:
#   nix run .#pcp-test-all            # Container + K8s + MicroVM tests
#
# Individual Tests:
#   nix run .#pcp-container-test      # Docker/Podman lifecycle
#   nix run .#pcp-k8s-test            # Kubernetes DaemonSet (needs minikube)
#   nix run .#pcp-test-all-microvms   # All MicroVM variants
#
# MicroVM with TAP networking (for Grafana dashboards):
#   nix run .#pcp-check-host                # Verify host environment
#   sudo nix run .#pcp-network-setup        # Create TAP bridge (requires sudo)
#   nix build .#pcp-microvm-grafana-tap && ./result/bin/microvm-run
#   # Access Grafana at http://10.177.0.20:3000
#   nix run .#pcp-vm-stop                   # Stop VM
#   sudo nix run .#pcp-network-teardown     # Cleanup (requires sudo)
#
# See also: ./docs/HowTos/nix/index.rst
#
{
  description = "Performance Co-Pilot (PCP) - system performance monitoring toolkit";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    flake-utils.url = "github:numtide/flake-utils";
    microvm = {
      url = "github:astro/microvm.nix";
      inputs.nixpkgs.follows = "nixpkgs";
    };
  };

  outputs =
    {
      self,
      nixpkgs,
      flake-utils,
      microvm,
    }:
    flake-utils.lib.eachDefaultSystem (
      system:
      let
        pkgs = nixpkgs.legacyPackages.${system};
        lib = pkgs.lib;

        # Import modular package definition
        # Pass self for stable source hashing - see nix/package.nix for details
        pcp = import ./nix/package.nix { inherit pkgs; src = self; };

        # Import shared constants and variant definitions
        constants = import ./nix/constants.nix;
        variants = import ./nix/variants.nix { inherit constants; };
        nixosModule = import ./nix/nixos-module.nix;

        # ─── MicroVM Generator ───────────────────────────────────────────
        # Creates a MicroVM runner with the specified configuration.
        # See nix/microvm.nix for full parameter documentation.
        mkMicroVM = {
          networking ? "user",
          debugMode ? true,
          enablePmlogger ? true,
          enableEvalTools ? false,
          enablePmieTest ? false,
          enableGrafana ? false,
          enableBpf ? false,
          enableBcc ? false,
          portOffset ? 0,
          variant ? "base",
        }:
          import ./nix/microvm.nix {
            inherit pkgs lib pcp microvm nixosModule nixpkgs system;
            inherit networking debugMode enablePmlogger enableEvalTools
                    enablePmieTest enableGrafana enableBpf enableBcc
                    portOffset variant;
          };

        # ─── Variant Package Generator ───────────────────────────────────
        # Generates MicroVM packages for all variants and networking modes.
        mkVariantPackages = lib.foldl' (acc: variantName:
          let
            def = variants.definitions.${variantName};
            portOffset = constants.variantPortOffsets.${variantName};

            # User-mode networking variant
            userPkg = {
              name = variants.mkPackageName variantName "user";
              value = mkMicroVM ({
                networking = "user";
                inherit portOffset;
                variant = variantName;
              } // def.config);
            };

            # TAP networking variant (if supported)
            tapPkg = lib.optionalAttrs def.supportsTap {
              name = variants.mkPackageName variantName "tap";
              value = mkMicroVM ({
                networking = "tap";
                inherit portOffset;
                variant = variantName;
              } // def.config);
            };
          in
            acc // { ${userPkg.name} = userPkg.value; }
            // lib.optionalAttrs (tapPkg ? name) { ${tapPkg.name} = tapPkg.value; }
        ) {} variants.variantNames;

        # Import lifecycle testing framework (Linux only)
        lifecycle = lib.optionalAttrs pkgs.stdenv.isLinux (
          import ./nix/lifecycle { inherit pkgs lib; }
        );

        # Import container module (Linux only) - returns { image, inputsHash }
        containerModule = lib.optionalAttrs pkgs.stdenv.isLinux (
          import ./nix/container.nix { inherit pkgs pcp; }
        );

        # Import container testing framework (Linux only)
        containerTest = lib.optionalAttrs pkgs.stdenv.isLinux (
          import ./nix/container-test {
            inherit pkgs lib pcp;
            containerInputsHash = containerModule.inputsHash or "";
          }
        );

        # Import Kubernetes testing framework (Linux only)
        k8sTest = lib.optionalAttrs pkgs.stdenv.isLinux (
          import ./nix/k8s-test {
            inherit pkgs lib pcp;
            containerInputsHash = containerModule.inputsHash or "";
          }
        );

        # Import test-all runner (Linux only)
        testAll = lib.optionalAttrs pkgs.stdenv.isLinux (
          import ./nix/test-all {
            inherit pkgs lib containerTest k8sTest;
          }
        );

      in
      {
        packages = {
          default = pcp;
          inherit pcp;
        } // lib.optionalAttrs pkgs.stdenv.isLinux (
          {
            # OCI container image (Linux only)
            pcp-container = containerModule.image;
          }
          # MicroVM packages for all variants
          // mkVariantPackages
          # Lifecycle testing packages
          // lifecycle.packages
          # Container testing packages
          // containerTest.packages
          # Kubernetes testing packages
          // k8sTest.packages
          # Test-all runner
          // testAll.packages
        );

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

            # ─── MicroVM Test Apps ────────────────────────────────────────────
            # Generate test apps for each variant
            mkTestApp = variant: networkMode:
              let
                testName = variants.mkTestAppName variant networkMode;
                isTap = networkMode == "tap";
                portOffset = constants.variantPortOffsets.${variant};
                sshPort = constants.ports.sshForward + portOffset;
                host = if isTap then constants.network.vmIp else "localhost";
              in {
                name = testName;
                value = {
                  type = "app";
                  program = "${import ./nix/tests/microvm-test.nix {
                    inherit pkgs lib;
                    variant = "${variant}-${networkMode}";
                    inherit host sshPort;
                  }}/bin/pcp-test-${variant}-${networkMode}";
                };
              };

            # Generate test apps for all variants
            testApps = lib.foldl' (acc: variantName:
              let
                def = variants.definitions.${variantName};
                userTest = mkTestApp variantName "user";
                tapTest = lib.optionalAttrs def.supportsTap (mkTestApp variantName "tap");
              in
                acc // { ${userTest.name} = userTest.value; }
                // lib.optionalAttrs (tapTest ? name) { ${tapTest.name} = tapTest.value; }
            ) {} variants.variantNames;

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
            # Comprehensive test runner
            pcp-test-all-microvms = {
              type = "app";
              program = "${import ./nix/tests/test-all-microvms.nix { inherit pkgs lib; }}/bin/pcp-test-all-microvms";
            };
          }
          # Per-variant test apps
          // testApps
          # Lifecycle testing apps
          // lifecycle.apps
          # Container testing apps
          // containerTest.apps
          # Kubernetes testing apps
          // k8sTest.apps
          # Test-all runner
          // testAll.apps
        );
      }
    );
}
