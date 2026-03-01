# nix/test-common/inputs.nix
#
# Shared runtime inputs for PCP test scripts.
# Provides common packages needed by container and k8s tests.
#
# Usage:
#   inputs = import ../test-common/inputs.nix { inherit pkgs; };
#   runtimeInputs = inputs.common ++ inputs.container;
#
{ pkgs }:
rec {
  # Common runtime inputs for all test scripts
  common = with pkgs; [
    coreutils
    gnugrep
    gnused
    gawk
    procps
    netcat-gnu
    bc
    util-linux
    nix
  ];

  # Additional inputs for container tests (docker/podman)
  container = with pkgs; [
    docker
  ];

  # Additional inputs for Kubernetes tests
  k8s = with pkgs; [
    jq
    kubectl
    minikube
    docker
  ];

  # All container test inputs combined
  containerAll = common ++ container;

  # All k8s test inputs combined
  k8sAll = common ++ k8s;
}
