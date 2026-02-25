# nix/container-test/constants.nix
#
# Container lifecycle testing configuration.
# Provides constants for OCI container testing phases.
#
{ }:
let
  mainConstants = import ../constants.nix;
in
rec {
  # Re-export relevant constants from main
  inherit (mainConstants) ports;

  # ─── Container Settings ─────────────────────────────────────────────────
  container = {
    name = "pcp-test";
    imageName = "pcp";
    imageTag = "latest";
  };

  # ─── Timeouts (seconds) ─────────────────────────────────────────────────
  timeouts = {
    build = 300;      # Phase 0: Build container image
    load = 60;        # Phase 1: Load image into runtime
    start = 10;       # Phase 2: Start container
    ready = 30;       # Phase 3-5: Wait for services/ports/metrics
    shutdown = 30;    # Phase 6: Graceful shutdown
    forceKill = 5;    # Phase 7: Force kill timeout
    cleanup = 5;      # Phase 7: Cleanup
  };

  # ─── Verification Checks ────────────────────────────────────────────────
  checks = {
    # Processes to verify inside container
    processes = [ "pmcd" ];

    # Metrics to verify via pminfo -h localhost
    metrics = [
      "kernel.all.load"
      "kernel.all.cpu.user"
      "mem.physmem"
    ];
  };

  # ─── Phase Definitions ──────────────────────────────────────────────────
  phases = {
    "0" = { name = "Build Image"; description = "Build OCI image with nix build"; };
    "1" = { name = "Load Image"; description = "Load image into docker/podman"; };
    "2" = { name = "Start Container"; description = "Run container with port mappings"; };
    "3" = { name = "Verify Process"; description = "Check pmcd process is running"; };
    "4" = { name = "Verify Ports"; description = "Check ports 44321/44322 are listening"; };
    "5" = { name = "Verify Metrics"; description = "Query metrics via pminfo -h localhost"; };
    "6" = { name = "Shutdown"; description = "Graceful docker stop with timeout"; };
    "7" = { name = "Cleanup"; description = "Remove container"; };
  };

  # ─── Terminal Formatting ────────────────────────────────────────────────
  colors = {
    reset = "\\033[0m";
    bold = "\\033[1m";
    red = "\\033[31m";
    green = "\\033[32m";
    yellow = "\\033[33m";
    blue = "\\033[34m";
    cyan = "\\033[36m";
  };
}
