# nix/k8s-manifests/constants.nix
#
# Kubernetes deployment constants for PCP DaemonSet manifests.
# These are the production defaults; test suites can override
# (e.g. namespace -> "pcp-test") via the namespaceOverride parameter
# in default.nix.
#
{ }:
let
  mainConstants = import ../constants.nix;
in
{
  # Re-export ports from main constants
  inherit (mainConstants) ports;

  # ─── Kubernetes Deployment Settings ──────────────────────────────────
  k8s = {
    namespace = "pcp";
    daemonSetName = "pcp";

    image = {
      name = "pcp";
      tag = "latest";
      pullPolicy = "Never";
    };

    resources = {
      limits = {
        memory = "512Mi";
        cpu = "500m";
      };
      requests = {
        memory = "256Mi";
        cpu = "100m";
      };
    };

    hostMounts = {
      root = "/host";
      proc = "/host/proc";
      sys = "/host/sys";
      kernelDebug = "/sys/kernel/debug";
    };
  };
}
