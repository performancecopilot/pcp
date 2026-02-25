# nix/lifecycle/constants.nix
#
# Lifecycle testing configuration for PCP MicroVMs.
# Extends the main constants.nix with lifecycle-specific values.
#
# This file provides:
# - Per-variant service and metric check configurations
# - Phase descriptions and expected outputs
# - Color codes and formatting for terminal output
#
{ }:
let
  # Import main constants for shared values
  mainConstants = import ../constants.nix;
in
rec {
  # Re-export relevant main constants
  inherit (mainConstants)
    network ports console variantPortOffsets
    getConsolePorts getHostname getProcessName getTimeout
    lifecycle;

  # ─── Variant Definitions ──────────────────────────────────────────────
  # Each variant specifies which services and metrics to verify.
  #
  variants = {
    base = {
      description = "Base PCP (pmcd, pmlogger, pmproxy)";
      services = [ "pmcd" "pmproxy" "pmlogger" ];
      metrics = [ "kernel.all.load" "kernel.all.cpu.user" "mem.physmem" ];
      httpChecks = [
        { name = "pmproxy"; port = ports.pmproxy; path = "/pmapi/1/metrics?target=kernel.all.load"; }
      ];
    };

    eval = {
      description = "Evaluation (+ node_exporter, below, pmie-test)";
      services = [ "pmcd" "pmproxy" "prometheus-node-exporter" "pmie-test" "stress-ng-test" ];
      metrics = [ "kernel.all.load" "kernel.all.cpu.user" "mem.physmem" ];
      httpChecks = [
        { name = "pmproxy"; port = ports.pmproxy; path = "/pmapi/1/metrics?target=kernel.all.load"; }
        { name = "node_exporter"; port = ports.nodeExporter; path = "/metrics"; }
      ];
    };

    grafana = {
      description = "Grafana (+ Prometheus + BPF dashboards)";
      services = [ "pmcd" "pmproxy" "prometheus-node-exporter" "grafana" "prometheus" ];
      metrics = [
        "kernel.all.load" "kernel.all.cpu.user" "mem.physmem"
        # BPF PMDA metrics (grafana variant has enableBpf=true for BPF dashboards)
        "bpf.runq.latency" "bpf.disk.all.latency"
      ];
      httpChecks = [
        { name = "pmproxy"; port = ports.pmproxy; path = "/pmapi/1/metrics?target=kernel.all.load"; }
        { name = "node_exporter"; port = ports.nodeExporter; path = "/metrics"; }
        { name = "grafana"; port = ports.grafana; path = "/api/health"; }
        { name = "prometheus"; port = ports.prometheus; path = "/-/ready"; }
      ];
    };

    bpf = {
      description = "BPF PMDA (pre-compiled eBPF)";
      services = [ "pmcd" "pmproxy" "prometheus-node-exporter" ];
      metrics = [
        "kernel.all.load" "kernel.all.cpu.user" "mem.physmem"
        # BPF PMDA metrics (runqlat -> bpf.runq.latency, biolatency -> bpf.disk.all.latency)
        "bpf.runq.latency" "bpf.disk.all.latency"
      ];
      httpChecks = [
        { name = "pmproxy"; port = ports.pmproxy; path = "/pmapi/1/metrics?target=kernel.all.load"; }
        { name = "node_exporter"; port = ports.nodeExporter; path = "/metrics"; }
      ];
    };

    # NOTE: BCC is deprecated - use BPF PMDA instead.
    # BCC used runtime eBPF compilation which is slower and less reliable
    # than the pre-compiled BPF PMDA CO-RE approach.
    # bcc = {
    #   description = "BCC PMDA (runtime eBPF compilation)";
    #   services = [ "pmcd" "pmproxy" "prometheus-node-exporter" ];
    #   metrics = [
    #     "kernel.all.load" "kernel.all.cpu.user" "mem.physmem"
    #   ];
    #   bccMetrics = [ "bcc.runqlat" "bcc.biolatency" ];
    #   httpChecks = [
    #     { name = "pmproxy"; port = ports.pmproxy; path = "/pmapi/1/metrics?target=kernel.all.load"; }
    #     { name = "node_exporter"; port = ports.nodeExporter; path = "/metrics"; }
    #   ];
    # };
  };

  # ─── Phase Definitions ────────────────────────────────────────────────
  # Human-readable descriptions for each lifecycle phase.
  #
  phases = {
    "0" = { name = "Build VM"; description = "Build the MicroVM derivation"; };
    "1" = { name = "Start VM"; description = "Start QEMU process and verify it's running"; };
    "2" = { name = "Serial Console"; description = "Verify serial console (ttyS0) is responsive"; };
    "2b" = { name = "Virtio Console"; description = "Verify virtio console (hvc0) is responsive"; };
    "3" = { name = "Verify Services"; description = "Check PCP and related services are active"; };
    "4" = { name = "Verify Metrics"; description = "Check PCP metrics are available"; };
    "5" = { name = "Shutdown"; description = "Send shutdown command via console"; };
    "6" = { name = "Wait Exit"; description = "Wait for VM process to exit cleanly"; };
  };

  # ─── Terminal Formatting ──────────────────────────────────────────────
  # ANSI color codes for terminal output.
  #
  colors = {
    reset = "\\033[0m";
    bold = "\\033[1m";
    red = "\\033[31m";
    green = "\\033[32m";
    yellow = "\\033[33m";
    blue = "\\033[34m";
    cyan = "\\033[36m";
  };

  # ─── Expect Script Configuration ──────────────────────────────────────
  # Configuration for expect scripts that interact with serial/virtio consoles.
  #
  expect = {
    # Login prompt patterns
    loginPrompt = "login:";

    # Shell prompt patterns (after login)
    # Matches: root@pcp-vm:~# or root@pcp-eval-vm:~# etc.
    shellPromptPattern = "root@pcp-.*-?vm:.*#";

    # Boot completion marker (systemd target reached)
    bootCompletePattern = "Reached target.*Multi-User System|Welcome to NixOS";

    # Default username and password for debug mode VMs
    username = "root";
    password = "pcp";

    # Timeout for expect operations (seconds)
    defaultTimeout = 30;

    # Time to wait between sending characters (milliseconds)
    sendDelay = 50;
  };

  # ─── Shutdown Commands ────────────────────────────────────────────────
  # Commands used to gracefully shut down the VM.
  #
  shutdown = {
    # Primary shutdown command
    command = "poweroff";

    # Alternative if poweroff hangs
    alternative = "systemctl poweroff --force";

    # Pattern indicating shutdown has begun
    shutdownPattern = "System is going down|Powering off|Reached target.*Shutdown";
  };
}
