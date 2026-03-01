# nix/k8s-test/constants.nix
#
# Kubernetes DaemonSet testing configuration.
# Provides constants for Minikube-based PCP testing with BPF metrics.
#
{ }:
let
  common = import ../test-common/constants.nix { };
in
rec {
  # Re-export from shared constants
  inherit (common) ports user colors;

  # ─── Kubernetes Settings ───────────────────────────────────────────────
  k8s = {
    namespace = "pcp-test";
    daemonSetName = "pcp";
    imageName = "pcp";
    imageTag = "latest";
  };

  # ─── Timeouts (seconds) ────────────────────────────────────────────────
  timeouts = {
    prerequisites = 30;   # Phase 0: Check minikube/kubectl
    build = common.timeouts.build;
    load = 120;           # Phase 2: Load image into minikube (longer for first load)
    deploy = 30;          # Phase 3: Apply DaemonSet manifest
    podsReady = 120;      # Phase 4: Wait for pods to be ready
    verify = common.timeouts.ready;
    metrics = 60;         # Phase 7: Verify kernel metrics
    bpfMetrics = 90;      # Phase 8: BPF metrics need time to collect data
    cleanup = common.timeouts.shutdown;
  };

  # ─── Minikube Recommended Settings ─────────────────────────────────────
  minikube = {
    driver = "docker";    # Docker driver - most reliable, works everywhere
    cpus = 4;             # More CPUs for parallel workloads
    memory = 8192;        # 8GB RAM for comfortable operation
    diskSize = "20g";     # 20GB disk
  };

  # ─── Verification Checks ───────────────────────────────────────────────
  checks = {
    # Processes to verify inside pods
    processes = [ "pmcd" ];

    # Basic metrics (always available)
    basicMetrics = common.metrics.basic;

    # Kernel metrics (require host /proc access)
    kernelMetrics = common.metrics.kernel;

    # BPF metrics (require privileged + BTF kernel)
    bpfMetrics = common.metrics.bpf;

    # Ports to verify
    ports = [ 44321 ];
  };

  # ─── Phase Definitions ─────────────────────────────────────────────────
  phases = {
    "0" = { name = "Prerequisites"; description = "Check minikube and kubectl"; };
    "1" = { name = "Build Image"; description = "Build OCI image with nix build"; };
    "2" = { name = "Load Image"; description = "Load image into minikube"; };
    "3" = { name = "Deploy DaemonSet"; description = "Apply privileged DaemonSet manifest"; };
    "4" = { name = "Wait Pods Ready"; description = "Wait for all DaemonSet pods"; };
    "5" = { name = "Verify Process"; description = "Check pmcd running in each pod"; };
    "6" = { name = "Verify Ports"; description = "Check port 44321 in each pod"; };
    "7" = { name = "Verify Kernel Metrics"; description = "Query kernel.all.load, mem.physmem"; };
    "8" = { name = "Verify BPF Metrics"; description = "Query bpf.runq.latency, bpf.disk.all.latency"; };
    "9" = { name = "Cleanup"; description = "Delete namespace and resources"; };
  };
}
