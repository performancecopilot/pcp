# nix/constants.nix
#
# Shared constants for PCP MicroVM infrastructure.
# Import this file in microvm.nix, network-setup.nix, test-lib.nix, etc.
# to ensure all configurations stay synchronized.
#
# DESIGN: These are defaults that can be overridden via module options.
# The constants file provides consistency; the NixOS module provides flexibility.
# See services.pcp.ports.*, services.pcp.network.* for user-facing overrides.
#
# PORT ALLOCATION: Each MicroVM variant gets a unique port offset to avoid
# conflicts when testing. See variantPortOffsets below.
#
rec {
  # ─── Network Configuration ─────────────────────────────────────────────
  # These are defaults. For custom deployments, override via module options.
  network = {
    # TAP networking
    bridge = "pcpbr0";
    tap = "pcptap0";
    subnet = "10.177.0.0/24";
    gateway = "10.177.0.1";
    vmIp = "10.177.0.20";

    # VM MAC address (consistent across all variants)
    vmMac = "02:00:00:0a:cb:01";
  };

  # ─── Port Configuration ────────────────────────────────────────────────
  # Standard PCP ports. These match upstream defaults.
  ports = {
    pmcd = 44321;
    pmproxy = 44322;
    nodeExporter = 9100;
    prometheus = 9090;
    grafana = 3000;

    # Host-side port forwarding (user-mode networking)
    # These avoid conflicts with services running on the host
    sshForward = 22022;
    prometheusForward = 19090;
    grafanaForward = 13000;
  };

  # ─── VM Resources ──────────────────────────────────────────────────────
  vm = {
    memoryMB = 1024;
    vcpus = 4;
  };

  # ─── Security Configuration ────────────────────────────────────────────
  security = {
    # systemd-analyze security score thresholds
    # These are enforced during testing; adjust if hardening changes scores
    networkServiceMaxScore = 5.0;   # pmcd, pmproxy must be <= this
    internalServiceWarnScore = 7.0; # pmlogger, pmie warn if > this

    # SSH defaults for MicroVM
    sshAllowPasswordAuth = false;   # Require key-based auth
    sshPermitRootLogin = "prohibit-password";  # Keys only
  };

  # ─── PMDA Domain IDs ───────────────────────────────────────────────────
  # These are assigned in PCP source: src/pmdas/<name>/domain.h
  # Must match the PMNS definitions in libexec/pcp/pmns/root_<name>
  pmdaDomains = {
    pmcd = 2;      # src/pmdas/pmcd/domain.h
    linux = 60;    # src/pmdas/linux/domain.h
    bpf = 157;     # src/pmdas/bpf/domain.h (pre-compiled CO-RE eBPF)
    # NOTE: BCC is deprecated - use BPF PMDA instead (CO-RE eBPF)
    # bcc = 149;   # src/pmdas/bcc/domain.h (runtime-compiled eBPF)
  };

  # ─── Test Configuration ────────────────────────────────────────────────
  test = {
    sshTimeoutSeconds = 5;
    sshMaxAttempts = 60;        # 60 * 2s = 120s max wait
    sshRetryDelaySeconds = 2;
    metricParityTolerancePct = 10;
    minExpectedMetrics = 1000;  # Typical linux PMDA set
    buildPollSeconds = 10;      # Poll interval for build completion
  };

  # ─── Variant Port Offsets ─────────────────────────────────────────────
  # Each MicroVM variant gets a unique port offset to avoid conflicts.
  # All forwarded ports (SSH, pmcd, pmproxy, etc.) are shifted by this offset.
  # This allows multiple variants to run simultaneously for testing.
  #
  # Usage: actual_port = base_port + variantPortOffsets.<variant>
  #
  # TAP variants use direct IP access (10.177.0.20) and share offset with
  # their user-mode counterpart (only one TAP VM can run at a time anyway).
  #
  variantPortOffsets = {
    base = 0;       # pcp-microvm, pcp-microvm-tap
    eval = 100;     # pcp-microvm-eval, pcp-microvm-eval-tap
    grafana = 200;  # pcp-microvm-grafana, pcp-microvm-grafana-tap
    bpf = 300;      # pcp-microvm-bpf
    # NOTE: BCC is deprecated - use BPF PMDA instead
    # bcc = 400;    # pcp-microvm-bcc
  };

  # ─── Serial Console Configuration ────────────────────────────────────
  # Each MicroVM variant gets TCP ports for serial console access.
  # This enables debugging early boot issues and network problems.
  #
  # Two console types:
  #   - serial (ttyS0): Traditional UART, slow but available immediately
  #   - virtio (hvc0): High-speed virtio-console, available after driver loads
  #
  # Port allocation scheme:
  #   Base port: 24500 (high port, unlikely to conflict)
  #   Each variant gets 10 ports:
  #     +0 = serial console (ttyS0)
  #     +1 = virtio console (hvc0)
  #     +2-9 = reserved for future use
  #
  # Connect with: nc localhost <port>
  # Or use: socat -,rawer tcp:localhost:<port>
  #
  console = {
    portBase = 24500;

    # Port offsets within each variant's block
    serialOffset = 0;   # ttyS0 - slow, early boot
    virtioOffset = 1;   # hvc0  - fast, after drivers load

    # Variant port blocks (10 ports each)
    variantBlocks = {
      base = 0;      # 24500-24509
      eval = 10;     # 24510-24519
      grafana = 20;  # 24520-24529
      bpf = 30;      # 24530-24539
      # NOTE: BCC is deprecated - use BPF PMDA instead
      # bcc = 40;    # 24540-24549
    };
  };

  # Helper function to get console ports for a variant
  # Usage: (getConsolePorts "base").serial  -> 24500
  #        (getConsolePorts "eval").virtio  -> 24511
  getConsolePorts = variant: {
    serial = console.portBase + console.variantBlocks.${variant} + console.serialOffset;
    virtio = console.portBase + console.variantBlocks.${variant} + console.virtioOffset;
  };

  # ─── Lifecycle Test Configuration ────────────────────────────────────
  # Timeouts and configuration for MicroVM lifecycle testing.
  # Each phase has a configurable timeout, with variant-specific overrides.
  #
  lifecycle = {
    # Poll interval for build/wait operations (seconds)
    pollInterval = 1;

    # Per-phase timeouts in seconds
    timeouts = {
      build = 600;           # Phase 0: Build VM (10 minutes)
      processStart = 5;      # Phase 1: Wait for QEMU process to appear
      serialReady = 30;      # Phase 2: Wait for serial console to be responsive
      virtioReady = 45;      # Phase 2b: Wait for virtio console (needs drivers)
      serviceReady = 60;     # Phase 3: Wait for services to be ready
      metricsReady = 30;     # Phase 4: Wait for metrics to be available
      shutdown = 30;         # Phase 5: Wait for shutdown command to complete
      waitExit = 60;         # Phase 6: Wait for process to exit cleanly
    };

    # Variant-specific timeout overrides (in seconds)
    variantTimeouts = {
      base = {};
      eval = {};
      grafana = { serviceReady = 90; };  # Grafana takes longer to start
      bpf = {};
      # NOTE: BCC is deprecated - use BPF PMDA instead (CO-RE eBPF)
      # BCC used runtime eBPF compilation which required longer timeouts
      # bcc = {
      #   serviceReady = 180;    # BCC modules compile at pmcd startup
      #   metricsReady = 120;    # BCC metrics appear after compilation
      # };
    };
  };

  # Helper function to get hostname for a variant
  # Usage: getHostname "base" -> "pcp-vm"
  #        getHostname "bpf"  -> "pcp-bpf-vm"
  getHostname = variant:
    if variant == "base" then "pcp-vm"
    else "pcp-${variant}-vm";

  # Helper function to get process name for a variant (same as hostname)
  # Used for pgrep matching against QEMU -name argument
  getProcessName = variant: getHostname variant;

  # Helper function to get timeout for a specific phase and variant
  # Returns variant-specific timeout if defined, otherwise default
  getTimeout = variant: phase:
    let
      variantOverrides = lifecycle.variantTimeouts.${variant} or {};
      defaultTimeout = lifecycle.timeouts.${phase};
    in
      variantOverrides.${phase} or defaultTimeout;
}
