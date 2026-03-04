# nix/k8s-manifests/kube-linter.nix
#
# KubeLinter validation for PCP Kubernetes manifests.
#
# Runs kube-linter (https://github.com/stackrox/kube-linter) at build time
# to catch manifest errors.  If someone introduces a change that triggers
# an unexpected kube-linter finding, the Nix build fails.
#
# ─── Why PCP suppresses certain kube-linter checks ────────────────────
#
# PCP is a *node-level monitoring agent*.  Its job is to collect system
# metrics (CPU, memory, disk I/O, network, per-process stats) and kernel
# trace data (via eBPF).  This is fundamentally different from a typical
# application workload — PCP needs deep access to the host it monitors.
#
# kube-linter's defaults assume a least-privilege application container.
# All 9 default findings are expected and intentional for PCP:
#
#   Check                         Why PCP needs it
#   ────────────────────────────  ──────────────────────────────────────
#   host-pid                      Required to see all node processes
#                                 for per-process metric collection
#                                 (proc.* metric namespace).
#
#   sensitive-host-mounts (x3)    Mounts /, /proc, /sys from the host.
#                                 This IS the monitoring — PCP reads
#                                 /proc for process/kernel metrics and
#                                 /sys for device/block metrics.
#
#   privileged-container          The BPF PMDA loads eBPF programs into
#   privilege-escalation          the kernel for tracing (runqueue
#                                 latency, disk I/O latency, etc.).
#                                 This requires CAP_SYS_ADMIN + BPF
#                                 access, which effectively means
#                                 privileged mode.
#
#   run-as-non-root               PCP services (pmcd, pmlogger) run as
#                                 root inside the container to access
#                                 BPF syscalls and host /proc.  The
#                                 container boundary provides isolation.
#
#   no-read-only-root-fs          PCP writes to /var/lib/pcp (PMNS
#                                 cache, PMDA state), /var/log/pcp
#                                 (pmlogger archives), and /run/pcp
#                                 (sockets).  Extensive tmpfs mounts
#                                 would add complexity with no security
#                                 benefit given privileged mode.
#
#   latest-tag                    The container image is built locally
#                                 by Nix (nix build .#pcp-container)
#                                 and loaded directly into minikube.
#                                 There is no registry; "latest" is
#                                 the only tag.
#
# The suppression rules live in kube-linter-config.yaml.  If PCP's
# deployment model changes (e.g. dropping BPF, using a registry),
# the corresponding suppression should be removed so kube-linter
# can enforce stricter defaults.
#
{ pkgs, manifestFiles }:
let
  lintConfig = ./kube-linter-config.yaml;

  fileArgs = builtins.concatStringsSep " " (map (f: "${f}") manifestFiles);
in
pkgs.runCommand "pcp-k8s-manifests-lint" {
  nativeBuildInputs = [ pkgs.kube-linter ];
} ''
  echo "Running kube-linter on PCP K8s manifests..."
  kube-linter lint \
    --config ${lintConfig} \
    ${fileArgs}
  echo "kube-linter: all checks passed" > $out
''
