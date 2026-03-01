# nix/k8s-test/lib.nix
#
# Shell helper functions for PCP Kubernetes DaemonSet testing.
# Provides k8s-specific operations on top of shared helpers.
#
{ pkgs, lib }:
let
  # Import shared helpers
  sharedHelpers = import ../test-common/shell-helpers.nix { };
  sharedInputs = import ../test-common/inputs.nix { inherit pkgs; };
in
rec {
  # Runtime inputs - use shared common + k8s-specific
  commonInputs = sharedInputs.common;
  k8sInputs = sharedInputs.k8s;

  # Re-export shared shell helpers
  inherit (sharedHelpers) colorHelpers timingHelpers;

  # ─── Kubernetes Helpers ────────────────────────────────────────────────
  k8sHelpers = ''
    # Check if minikube is running
    # Uses timeout to avoid hanging, with fallback to kubectl check
    minikube_running() {
      # First try minikube status with a short timeout
      if timeout 5 minikube status --format='{{.Host}}' 2>/dev/null | grep -q "Running"; then
        return 0
      fi

      # Fallback: check if kubectl can reach the minikube context
      if kubectl config current-context 2>/dev/null | grep -q "minikube"; then
        if timeout 5 kubectl cluster-info &>/dev/null; then
          return 0
        fi
      fi

      return 1
    }

    # Get number of nodes in the cluster
    get_node_count() {
      kubectl get nodes --no-headers 2>/dev/null | wc -l
    }

    # Load image into minikube
    load_image_to_minikube() {
      local image_path="$1"
      minikube image load "$image_path"
    }

    # Wait for DaemonSet to be ready
    # Returns 0 when desiredNumberScheduled == numberReady
    wait_daemonset_ready() {
      local namespace="$1"
      local name="$2"
      local timeout="$3"
      local elapsed=0

      while [[ $elapsed -lt $timeout ]]; do
        local status
        status=$(kubectl get daemonset "$name" -n "$namespace" -o json 2>/dev/null)
        if [[ -n "$status" ]]; then
          local desired ready
          desired=$(echo "$status" | jq -r '.status.desiredNumberScheduled // 0')
          ready=$(echo "$status" | jq -r '.status.numberReady // 0')

          if [[ "$desired" -gt 0 && "$desired" == "$ready" ]]; then
            echo "$ready"
            return 0
          fi
        fi

        sleep 2
        elapsed=$((elapsed + 2))
      done

      return 1
    }

    # Get all pods for a DaemonSet
    get_daemonset_pods() {
      local namespace="$1"
      local name="$2"
      kubectl get pods -n "$namespace" -l "app=$name" -o jsonpath='{.items[*].metadata.name}' 2>/dev/null
    }

    # Get pod's node name
    get_pod_node() {
      local namespace="$1"
      local pod="$2"
      kubectl get pod "$pod" -n "$namespace" -o jsonpath='{.spec.nodeName}' 2>/dev/null
    }

    # Execute command in a pod
    kubectl_exec() {
      local namespace="$1"
      local pod="$2"
      shift 2
      kubectl exec -n "$namespace" "$pod" -- "$@" 2>/dev/null
    }

    # Check if pmcd process is running in pod
    check_process_in_pod() {
      local namespace="$1"
      local pod="$2"
      local proc="$3"

      if [[ "$proc" == "pmcd" ]]; then
        # For pmcd, verify it's responding to requests
        if kubectl_exec "$namespace" "$pod" pminfo -f pmcd.version &>/dev/null; then
          return 0
        fi
      fi

      # Try pgrep if available
      if kubectl_exec "$namespace" "$pod" pgrep -x "$proc" &>/dev/null; then
        return 0
      fi

      # Fallback: check /proc/1/comm
      local comm
      comm=$(kubectl_exec "$namespace" "$pod" cat /proc/1/comm 2>/dev/null || true)
      [[ "$comm" == "$proc" ]]
    }

    # Check if port is listening in pod
    check_port_in_pod() {
      local namespace="$1"
      local pod="$2"
      local port="$3"

      if [[ "$port" == "44321" ]]; then
        # For pmcd port, verify by querying pmcd
        if kubectl_exec "$namespace" "$pod" pminfo -f pmcd.version &>/dev/null; then
          return 0
        fi
      fi

      # Try ss if available
      if kubectl_exec "$namespace" "$pod" ss -tln 2>/dev/null | grep -q ":$port "; then
        return 0
      fi

      # Try netstat if available
      if kubectl_exec "$namespace" "$pod" netstat -tln 2>/dev/null | grep -q ":$port "; then
        return 0
      fi

      return 1
    }

    # Create namespace if it doesn't exist
    ensure_namespace() {
      local namespace="$1"
      if ! kubectl get namespace "$namespace" &>/dev/null; then
        kubectl create namespace "$namespace"
      fi
    }

    # Get image ID from a docker tarball (nix-built container)
    get_tarball_image_id() {
      local tarball="$1"
      tar -xOf "$tarball" manifest.json 2>/dev/null | jq -r '.[0].Config' | sed 's/\.json$//' || echo ""
    }

    # Get current image ID from docker
    get_docker_image_id() {
      local image="$1"
      docker images --no-trunc -q "$image" 2>/dev/null | head -1 | sed 's/sha256://' || echo ""
    }

    # Check if image needs to be loaded (compares hashes)
    image_needs_loading() {
      local tarball="$1"
      local image="$2"

      local tarball_id docker_id
      tarball_id=$(get_tarball_image_id "$tarball")
      docker_id=$(get_docker_image_id "$image")

      # If we can't get the tarball ID, always load
      [[ -z "$tarball_id" ]] && return 0

      # If docker doesn't have the image, need to load
      [[ -z "$docker_id" ]] && return 0

      # Compare IDs
      [[ "$tarball_id" != "$docker_id" ]]
    }

    # Delete namespace and wait for cleanup
    cleanup_namespace() {
      local namespace="$1"
      local timeout="$2"
      local elapsed=0

      if kubectl get namespace "$namespace" &>/dev/null; then
        kubectl delete namespace "$namespace" --wait=false &>/dev/null || true

        # Wait for namespace to be deleted
        while kubectl get namespace "$namespace" &>/dev/null; do
          sleep 2
          elapsed=$((elapsed + 2))
          if [[ $elapsed -ge $timeout ]]; then
            warn "Namespace deletion timed out"
            return 1
          fi
        done
      fi
      return 0
    }
  '';

  # ─── Metric Verification Helpers ───────────────────────────────────────
  metricHelpers = ''
    # Check if a metric is available and has actual values via pminfo inside the pod
    check_metric_in_pod() {
      local namespace="$1"
      local pod="$2"
      local metric="$3"
      local output
      output=$(kubectl_exec "$namespace" "$pod" pminfo -f "$metric" 2>/dev/null)
      # Check that we got output AND it contains a value (not just the metric name)
      [[ -n "$output" ]] && echo "$output" | grep -qE '(value|inst)'
    }

    # Check BPF metric with retry (histograms need time to populate)
    check_bpf_metric() {
      local namespace="$1"
      local pod="$2"
      local metric="$3"
      local max_retries="$4"
      local retry=0

      while [[ $retry -lt $max_retries ]]; do
        local output
        output=$(kubectl_exec "$namespace" "$pod" pminfo -f "$metric" 2>/dev/null || true)

        # Check if we got histogram data (contains "inst" or actual values)
        if [[ -n "$output" ]] && echo "$output" | grep -qE '(inst|value)'; then
          # Count buckets/instances
          local count
          count=$(echo "$output" | grep -c 'inst' || echo "0")
          if [[ "$count" -gt 0 ]]; then
            echo "$count"
            return 0
          fi
        fi

        sleep 5
        retry=$((retry + 1))
      done

      return 1
    }

    # Check if BTF is available (required for BPF PMDA)
    check_btf_available() {
      local namespace="$1"
      local pod="$2"
      kubectl_exec "$namespace" "$pod" test -f /sys/kernel/btf/vmlinux 2>/dev/null
    }

    # Get metric value (for display)
    get_metric_value() {
      local namespace="$1"
      local pod="$2"
      local metric="$3"
      kubectl_exec "$namespace" "$pod" pminfo -f "$metric" 2>/dev/null | grep -E '(value|inst)' | head -3
    }
  '';

  # ─── Combined Helpers ──────────────────────────────────────────────────
  # All helpers combined for use in test scripts
  allHelpers = lib.concatStringsSep "\n" [
    colorHelpers
    timingHelpers
    k8sHelpers
    metricHelpers
  ];
}
