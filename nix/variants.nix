# nix/variants.nix
#
# Centralized MicroVM variant definitions for PCP.
#
# This file defines all MicroVM variants in one place, eliminating duplication
# in flake.nix. Each variant specifies its configuration options and whether
# it supports TAP networking.
#
# USAGE:
#   variants = import ./nix/variants.nix { inherit constants; };
#   variants.definitions.grafana.config  # -> { enableGrafana = true; ... }
#   variants.mkPackageName "grafana" "tap"  # -> "pcp-microvm-grafana-tap"
#
# ADDING A NEW VARIANT:
#   1. Add entry to definitions below
#   2. Add port offset in constants.nix (variantPortOffsets)
#   3. Add console block in constants.nix (console.variantBlocks)
#   4. Run: nix flake show (verify new packages appear)
#
{ constants }:
rec {
  # ─── Variant Definitions ────────────────────────────────────────────────
  # Each variant specifies:
  #   - description: Human-readable description for documentation
  #   - config: Options passed to mkMicroVM (merged with defaults)
  #   - supportsTap: Whether to generate a TAP networking variant
  #
  definitions = {
    base = {
      description = "Base PCP (pmcd, pmlogger, pmproxy)";
      config = {};  # Uses defaults
      supportsTap = true;
    };

    eval = {
      description = "Evaluation (+ node_exporter, pmie-test)";
      config = {
        enablePmlogger = false;
        enableEvalTools = true;
        enablePmieTest = true;
      };
      supportsTap = true;
    };

    grafana = {
      description = "Grafana (+ Prometheus + BPF dashboards)";
      config = {
        enablePmlogger = false;
        enableEvalTools = true;
        enablePmieTest = true;
        enableGrafana = true;
        enableBpf = true;
      };
      supportsTap = true;
    };

    bpf = {
      description = "BPF PMDA (pre-compiled eBPF)";
      config = {
        enablePmlogger = false;
        enableEvalTools = true;
        enablePmieTest = true;
        enableBpf = true;
      };
      supportsTap = false;
    };
  };

  # ─── Helper Functions ───────────────────────────────────────────────────

  # Generate package name for a variant and networking mode
  # mkPackageName "base" "user" -> "pcp-microvm"
  # mkPackageName "base" "tap"  -> "pcp-microvm-tap"
  # mkPackageName "grafana" "user" -> "pcp-microvm-grafana"
  # mkPackageName "grafana" "tap"  -> "pcp-microvm-grafana-tap"
  mkPackageName = variant: networking:
    let
      base = if variant == "base" then "pcp-microvm" else "pcp-microvm-${variant}";
    in
      if networking == "tap" then "${base}-tap" else base;

  # Generate test app name for a variant and networking mode
  # mkTestAppName "base" "user" -> "pcp-test-base-user"
  # mkTestAppName "grafana" "tap" -> "pcp-test-grafana-tap"
  mkTestAppName = variant: networking:
    "pcp-test-${variant}-${networking}";

  # List of all variant names
  variantNames = builtins.attrNames definitions;
}
