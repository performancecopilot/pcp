# nix/test-common/constants.nix
#
# Shared constants for PCP test scripts.
# Provides common values used by container and k8s tests.
#
# Usage:
#   common = import ../test-common/constants.nix { };
#   inherit (common) colors metrics;
#
{ }:
let
  mainConstants = import ../constants.nix;
in
rec {
  # Re-export from main constants
  inherit (mainConstants) ports user;

  # ─── Terminal Formatting ────────────────────────────────────────────────
  # ANSI escape codes for terminal colors (used in constants, not shell code)
  colors = {
    reset = "\\033[0m";
    bold = "\\033[1m";
    red = "\\033[31m";
    green = "\\033[32m";
    yellow = "\\033[33m";
    blue = "\\033[34m";
    cyan = "\\033[36m";
  };

  # ─── Common Metrics ─────────────────────────────────────────────────────
  # Metrics used by both container and k8s tests
  metrics = {
    # Basic PCP metrics (always available)
    basic = [
      "pmcd.version"
      "pmcd.numagents"
    ];

    # Kernel metrics (require host /proc access)
    kernel = [
      "kernel.all.load"
      "kernel.all.cpu.user"
      "mem.physmem"
    ];

    # BPF metrics (require privileged mode + BTF kernel)
    bpf = [
      "bpf.runq.latency"
      "bpf.disk.all.latency"
    ];
  };

  # ─── Common Timeouts ────────────────────────────────────────────────────
  # Default timeouts shared across tests (can be overridden)
  timeouts = {
    build = 300;      # Build container image
    ready = 30;       # Wait for services/ports/metrics
    shutdown = 30;    # Graceful shutdown
    cleanup = 5;      # Cleanup resources
  };
}
