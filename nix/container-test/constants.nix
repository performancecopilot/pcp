# nix/container-test/constants.nix
#
# Container lifecycle testing configuration.
# Provides constants for OCI container testing phases.
#
{ }:
let
  common = import ../test-common/constants.nix { };
in
rec {
  # Re-export from shared constants
  inherit (common) ports colors;

  # ─── Container Settings ─────────────────────────────────────────────────
  container = {
    name = "pcp-test";
    imageName = "pcp";
    imageTag = "latest";
  };

  # ─── Timeouts (seconds) ─────────────────────────────────────────────────
  timeouts = {
    build = common.timeouts.build;
    load = 60;        # Phase 1: Load image into runtime
    start = 10;       # Phase 2: Start container
    ready = common.timeouts.ready;
    shutdown = common.timeouts.shutdown;
    forceKill = 5;    # Phase 7: Force kill timeout
    cleanup = common.timeouts.cleanup;
  };

  # ─── Verification Checks ────────────────────────────────────────────────
  checks = {
    # Processes to verify inside container
    processes = [ "pmcd" ];

    # Basic metrics to verify via pminfo -h container
    metrics = common.metrics.basic ++ [ "pmcd.services" ];

    # Kernel metrics (require privileged mode for full /proc access)
    kernelMetrics = common.metrics.kernel;

    # BPF metrics (require privileged mode + BTF kernel)
    bpfMetrics = common.metrics.bpf;
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
}
