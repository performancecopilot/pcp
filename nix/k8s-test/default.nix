# nix/k8s-test/default.nix
#
# Entry point for PCP Kubernetes DaemonSet lifecycle testing.
# Generates test scripts for deploying PCP as a privileged DaemonSet
# in minikube with full node monitoring including BPF metrics.
#
# Usage in flake.nix:
#   k8sTest = import ./nix/k8s-test { inherit pkgs lib pcp; };
#
# Generated outputs:
#   k8sTest.packages.pcp-k8s-test       - Full lifecycle test
#   k8sTest.packages.pcp-k8s-test-quick - Quick test (skip build)
#   k8sTest.apps.pcp-k8s-test           - App entry point
#   k8sTest.apps.pcp-k8s-test-quick     - Quick app entry point
#
{ pkgs, lib, pcp, containerInputsHash }:
let
  constants = import ./constants.nix { };
  mainConstants = import ../constants.nix;
  helpers = import ./lib.nix { inherit pkgs lib; };
  manifests = import ./manifests.nix { inherit pkgs lib; };

  inherit (helpers)
    colorHelpers timingHelpers k8sHelpers metricHelpers
    commonInputs k8sInputs;

  # PCP tools for metric verification (use local package)
  pcpInputs = [ pcp ];

  # PCP_CONF path for pminfo
  pcpConfPath = "${pcp}/share/pcp/etc/pcp.conf";

  # ─── Full Lifecycle Test Script ────────────────────────────────────────
  # Tests the complete Kubernetes deployment lifecycle:
  # Prerequisites -> Build -> Load -> Deploy -> Verify (including BPF) -> Cleanup
  #
  mkFullTest = pkgs.writeShellApplication {
    name = "pcp-k8s-test";
    runtimeInputs = commonInputs ++ k8sInputs ++ pcpInputs;
    text = ''
      set +e  # Don't exit on first failure

      # Set PCP_CONF so pminfo can find its configuration
      export PCP_CONF="${pcpConfPath}"

      ${colorHelpers}
      ${timingHelpers}
      ${k8sHelpers}
      ${metricHelpers}

      # Configuration
      NAMESPACE="${constants.k8s.namespace}"
      DAEMONSET_NAME="${constants.k8s.daemonSetName}"
      RESULT_LINK="result-container"
      MANIFEST_FILE="${manifests.manifestFile}"

      # Metrics to verify (exported for potential external use)
      export KERNEL_METRICS="${lib.concatStringsSep " " constants.checks.kernelMetrics}"
      export BPF_METRICS="${lib.concatStringsSep " " constants.checks.bpfMetrics}"

      # Ports to verify
      PORTS_TO_CHECK="${lib.concatStringsSep " " (map toString constants.checks.ports)}"

      # Timing tracking
      declare -A PHASE_TIMES
      TOTAL_START=$(time_ms)

      # Results tracking
      TOTAL_PASSED=0
      TOTAL_FAILED=0
      TOTAL_SKIPPED=0
      BPF_AVAILABLE=true

      record_result() {
        local phase="$1"
        local passed="$2"
        local time_ms="$3"
        PHASE_TIMES["$phase"]=$time_ms
        if [[ "$passed" == "true" ]]; then
          TOTAL_PASSED=$((TOTAL_PASSED + 1))
        elif [[ "$passed" == "skip" ]]; then
          TOTAL_SKIPPED=$((TOTAL_SKIPPED + 1))
        else
          TOTAL_FAILED=$((TOTAL_FAILED + 1))
        fi
      }

      # Cleanup function
      cleanup() {
        info "Cleaning up..."
        cleanup_namespace "$NAMESPACE" ${toString constants.timeouts.cleanup} || true
        rm -f "$RESULT_LINK"
      }

      # Header
      bold "========================================"
      bold "  PCP Kubernetes DaemonSet Test"
      bold "========================================"
      echo ""

      # ─── Phase 0: Prerequisites ────────────────────────────────────────────
      phase_header "0" "Prerequisites" "${toString constants.timeouts.prerequisites}"
      start_time=$(time_ms)

      # Check minikube
      if minikube_running; then
        NODE_COUNT=$(get_node_count)
        elapsed=$(elapsed_ms "$start_time")
        result_pass "minikube running ($NODE_COUNT node(s))" "$elapsed"
        record_result "prerequisites" "true" "$elapsed"
      else
        elapsed=$(elapsed_ms "$start_time")
        result_fail "minikube not running" "$elapsed"
        error ""
        error "Please start minikube first. Options:"
        error ""
        error "  # Recommended: Use helper with optimal settings (KVM2, 4 CPUs, 8GB RAM):"
        error "  nix run .#pcp-minikube-start"
        error ""
        error "  # Or start manually:"
        error "  nix shell nixpkgs#minikube -c minikube start --driver=kvm2 --cpus=4 --memory=8192"
        error ""
        error "  # For multi-node testing:"
        error "  nix shell nixpkgs#minikube -c minikube start --driver=kvm2 --cpus=4 --memory=8192 --nodes=2"
        record_result "prerequisites" "false" "$elapsed"
        exit 1
      fi

      # Check kubectl
      if command -v kubectl &>/dev/null; then
        result_pass "kubectl available"
        TOTAL_PASSED=$((TOTAL_PASSED + 1))
      else
        result_fail "kubectl not found"
        TOTAL_FAILED=$((TOTAL_FAILED + 1))
        exit 1
      fi

      echo ""
      info "Using minikube cluster with $NODE_COUNT node(s)"

      # Quick connectivity check before slow operations
      info "Verifying minikube connectivity..."
      if ! timeout 10 kubectl cluster-info &>/dev/null; then
        error ""
        error "Kubernetes API not responding (10s timeout)."
        error "The minikube cluster may be unresponsive or have stale port mappings."
        error ""
        error "Try:"
        error "  minikube update-context   # Fix stale port mappings"
        error "  minikube stop && minikube start   # Restart cluster"
        error "  minikube delete && minikube start # Full reset"
        exit 1
      fi
      result_pass "Kubernetes API responsive"

      # Switch to minikube's docker daemon
      info "Connecting to minikube's docker daemon..."
      docker_env_start=$(time_ms)
      eval "$(minikube docker-env)"
      docker_env_elapsed=$(elapsed_ms "$docker_env_start")
      info "  Connected (''${docker_env_elapsed}ms)"
      PHASE_TIMES["docker_env"]=$docker_env_elapsed

      # ─── Phase 1: Build Image ──────────────────────────────────────────────
      phase_header "1" "Build Image" "${toString constants.timeouts.build}"
      start_time=$(time_ms)

      # Clean up any existing result link
      rm -f "$RESULT_LINK"

      info "  Building pcp-container..."
      if nix build ".#pcp-container" -o "$RESULT_LINK" 2>&1 | while read -r line; do
        echo "    $line"
      done; then
        elapsed=$(elapsed_ms "$start_time")
        result_pass "Image built" "$elapsed"
        record_result "build" "true" "$elapsed"
      else
        elapsed=$(elapsed_ms "$start_time")
        result_fail "Build failed" "$elapsed"
        record_result "build" "false" "$elapsed"
        exit 1
      fi

      # ─── Phase 2: Load Image ───────────────────────────────────────────────
      phase_header "2" "Load Image" "${toString constants.timeouts.load}"
      start_time=$(time_ms)

      # Fast cache check using Nix inputs hash label (docker-env already set above)
      EXPECTED_HASH="${containerInputsHash}"
      LOADED_HASH=""

      # Check if image exists and get its inputs hash label
      if docker image inspect "pcp:latest" &>/dev/null; then
        LOADED_HASH=$(docker inspect "pcp:latest" --format '{{index .Config.Labels "nix.inputs.hash"}}' 2>/dev/null || echo "")
      fi

      # Show cache check status
      if [[ -n "$LOADED_HASH" ]]; then
        info "  Cache: expected=$EXPECTED_HASH loaded=$LOADED_HASH"
      else
        info "  Cache: no existing image label found"
      fi

      if [[ -n "$EXPECTED_HASH" && "$EXPECTED_HASH" == "$LOADED_HASH" ]]; then
        elapsed=$(elapsed_ms "$start_time")
        result_pass "Image unchanged, skipping load" "$elapsed"
        record_result "load" "true" "$elapsed"
      else
        info "  Loading image into minikube's docker..."
        if docker load < "$RESULT_LINK" 2>&1 | while read -r line; do
          echo "    $line"
        done; then
          elapsed=$(elapsed_ms "$start_time")
          result_pass "Image loaded into minikube" "$elapsed"
          record_result "load" "true" "$elapsed"
        else
          elapsed=$(elapsed_ms "$start_time")
          result_fail "Failed to load image" "$elapsed"
          record_result "load" "false" "$elapsed"
          cleanup
          exit 1
        fi
      fi

      # ─── Phase 3: Deploy DaemonSet ─────────────────────────────────────────
      phase_header "3" "Deploy DaemonSet" "${toString constants.timeouts.deploy}"
      start_time=$(time_ms)

      # Clean up any existing deployment
      if kubectl get namespace "$NAMESPACE" &>/dev/null; then
        info "  Removing existing deployment..."
        cleanup_namespace "$NAMESPACE" ${toString constants.timeouts.cleanup}
        sleep 2
      fi

      info "  Applying DaemonSet manifest..."
      if kubectl apply -f "$MANIFEST_FILE" 2>&1 | while read -r line; do
        echo "    $line"
      done; then
        elapsed=$(elapsed_ms "$start_time")
        result_pass "DaemonSet deployed" "$elapsed"
        record_result "deploy" "true" "$elapsed"
      else
        elapsed=$(elapsed_ms "$start_time")
        result_fail "Failed to deploy DaemonSet" "$elapsed"
        record_result "deploy" "false" "$elapsed"
        cleanup
        exit 1
      fi

      # ─── Phase 4: Wait Pods Ready ──────────────────────────────────────────
      phase_header "4" "Wait Pods Ready" "${toString constants.timeouts.podsReady}"
      start_time=$(time_ms)

      info "  Waiting for DaemonSet pods to be ready..."
      if READY_COUNT=$(wait_daemonset_ready "$NAMESPACE" "$DAEMONSET_NAME" ${toString constants.timeouts.podsReady}); then
        elapsed=$(elapsed_ms "$start_time")
        result_pass "$READY_COUNT/$NODE_COUNT pods ready" "$elapsed"
        record_result "pods_ready" "true" "$elapsed"
      else
        elapsed=$(elapsed_ms "$start_time")
        result_fail "Pods not ready in time" "$elapsed"
        record_result "pods_ready" "false" "$elapsed"
        # Show pod status for debugging
        kubectl get pods -n "$NAMESPACE" -o wide
        kubectl describe pods -n "$NAMESPACE" | tail -50
        cleanup
        exit 1
      fi

      # Get list of pods for verification
      PODS=$(get_daemonset_pods "$NAMESPACE" "$DAEMONSET_NAME")
      info "  Pods: $PODS"

      # ─── Phase 5: Verify Process ───────────────────────────────────────────
      phase_header "5" "Verify Process" "${toString constants.timeouts.verify}"
      start_time=$(time_ms)

      for pod in $PODS; do
        node=$(get_pod_node "$NAMESPACE" "$pod")
        proc_start=$(time_ms)
        if check_process_in_pod "$NAMESPACE" "$pod" "pmcd"; then
          result_pass "pmcd running on $node ($pod)" "$(elapsed_ms "$proc_start")"
          TOTAL_PASSED=$((TOTAL_PASSED + 1))
        else
          result_fail "pmcd not running on $node ($pod)" "$(elapsed_ms "$proc_start")"
          TOTAL_FAILED=$((TOTAL_FAILED + 1))
        fi
      done

      elapsed=$(elapsed_ms "$start_time")
      record_result "process" "true" "$elapsed"

      # ─── Phase 6: Verify Ports ─────────────────────────────────────────────
      phase_header "6" "Verify Ports" "${toString constants.timeouts.verify}"
      start_time=$(time_ms)

      for pod in $PODS; do
        node=$(get_pod_node "$NAMESPACE" "$pod")
        for port in $PORTS_TO_CHECK; do
          port_start=$(time_ms)
          if check_port_in_pod "$NAMESPACE" "$pod" "$port"; then
            result_pass "Port $port on $node ($pod)" "$(elapsed_ms "$port_start")"
            TOTAL_PASSED=$((TOTAL_PASSED + 1))
          else
            result_fail "Port $port not listening on $node ($pod)" "$(elapsed_ms "$port_start")"
            TOTAL_FAILED=$((TOTAL_FAILED + 1))
          fi
        done
      done

      elapsed=$(elapsed_ms "$start_time")
      record_result "ports" "true" "$elapsed"

      # ─── Phase 7: Verify Kernel Metrics ────────────────────────────────────
      phase_header "7" "Verify Kernel Metrics" "${toString constants.timeouts.metrics}"
      start_time=$(time_ms)

      for pod in $PODS; do
        node=$(get_pod_node "$NAMESPACE" "$pod")
        info "  Node: $node"

        for metric in $KERNEL_METRICS; do
          met_start=$(time_ms)
          if check_metric_in_pod "$NAMESPACE" "$pod" "$metric"; then
            value=$(get_metric_value "$NAMESPACE" "$pod" "$metric" | head -1)
            result_pass "$metric = $value" "$(elapsed_ms "$met_start")"
            TOTAL_PASSED=$((TOTAL_PASSED + 1))
          else
            result_fail "$metric not available" "$(elapsed_ms "$met_start")"
            TOTAL_FAILED=$((TOTAL_FAILED + 1))
          fi
        done
      done

      elapsed=$(elapsed_ms "$start_time")
      record_result "kernel_metrics" "true" "$elapsed"

      # ─── Phase 8: Verify BPF Metrics ───────────────────────────────────────
      phase_header "8" "Verify BPF Metrics" "${toString constants.timeouts.bpfMetrics}"
      start_time=$(time_ms)

      # First check if BTF is available (required for BPF PMDA)
      first_pod=$(echo "$PODS" | awk '{print $1}')
      if ! check_btf_available "$NAMESPACE" "$first_pod"; then
        warn "  BTF not available - BPF metrics will be skipped"
        warn "  (Minikube kernel may not have CONFIG_DEBUG_INFO_BTF=y)"
        BPF_AVAILABLE=false
      fi

      # Also check if BPF PMDA is loaded
      if $BPF_AVAILABLE && ! check_metric_in_pod "$NAMESPACE" "$first_pod" "bpf"; then
        warn "  BPF PMDA not loaded - BPF metrics will be skipped"
        warn "  (Container may need BPF PMDA installation)"
        BPF_AVAILABLE=false
      fi

      if $BPF_AVAILABLE; then
        for pod in $PODS; do
          node=$(get_pod_node "$NAMESPACE" "$pod")
          info "  Node: $node"

          for metric in $BPF_METRICS; do
            met_start=$(time_ms)
            # BPF histogram metrics need retries as they populate over time
            if bucket_count=$(check_bpf_metric "$NAMESPACE" "$pod" "$metric" 6); then
              result_pass "$metric (histogram with $bucket_count buckets)" "$(elapsed_ms "$met_start")"
              TOTAL_PASSED=$((TOTAL_PASSED + 1))
            else
              result_fail "$metric not available" "$(elapsed_ms "$met_start")"
              TOTAL_FAILED=$((TOTAL_FAILED + 1))
            fi
          done
        done
      else
        for metric in $BPF_METRICS; do
          result_skip "$metric (BPF unavailable)"
          TOTAL_SKIPPED=$((TOTAL_SKIPPED + 1))
        done
      fi

      elapsed=$(elapsed_ms "$start_time")
      record_result "bpf_metrics" "true" "$elapsed"

      # ─── Phase 9: Cleanup ──────────────────────────────────────────────────
      phase_header "9" "Cleanup" "${toString constants.timeouts.cleanup}"
      start_time=$(time_ms)

      info "  Deleting namespace..."
      if cleanup_namespace "$NAMESPACE" ${toString constants.timeouts.cleanup}; then
        elapsed=$(elapsed_ms "$start_time")
        result_pass "Namespace deleted" "$elapsed"
        record_result "cleanup" "true" "$elapsed"
      else
        elapsed=$(elapsed_ms "$start_time")
        result_warn "Cleanup timed out" "$elapsed"
        record_result "cleanup" "true" "$elapsed"
      fi

      rm -f "$RESULT_LINK"

      # ─── Summary ───────────────────────────────────────────────────────────
      TOTAL_ELAPSED=$(elapsed_ms "$TOTAL_START")

      echo ""
      bold "  Timing Summary"
      echo "  $(printf '─%.0s' {1..37})"
      printf "  %-20s %10s\n" "Phase" "Time (ms)"
      echo "  $(printf '─%.0s' {1..37})"
      for phase in prerequisites docker_env build load deploy pods_ready process ports kernel_metrics bpf_metrics cleanup; do
        if [[ -n "''${PHASE_TIMES[$phase]:-}" ]]; then
          printf "  %-20s %10s\n" "$phase" "''${PHASE_TIMES[$phase]}"
        fi
      done
      echo "  $(printf '─%.0s' {1..37})"
      printf "  %-20s %10s\n" "TOTAL" "$TOTAL_ELAPSED"
      echo "  $(printf '─%.0s' {1..37})"

      echo ""
      bold "========================================"
      if [[ $TOTAL_FAILED -eq 0 ]]; then
        if [[ $TOTAL_SKIPPED -gt 0 ]]; then
          success "  Result: PASSED ($TOTAL_SKIPPED skipped)"
        else
          success "  Result: ALL PHASES PASSED"
        fi
        success "  Total time: $(format_ms "$TOTAL_ELAPSED")"
      else
        error "  Result: $TOTAL_FAILED CHECKS FAILED"
      fi
      bold "========================================"

      [[ $TOTAL_FAILED -eq 0 ]]
    '';
  };

  # ─── Quick Test Script ─────────────────────────────────────────────────
  # A faster test that skips the build phase (assumes image already loaded)
  #
  mkQuickTest = pkgs.writeShellApplication {
    name = "pcp-k8s-test-quick";
    runtimeInputs = commonInputs ++ k8sInputs ++ pcpInputs;
    text = ''
      set +e

      # Set PCP_CONF so pminfo can find its configuration
      export PCP_CONF="${pcpConfPath}"

      ${colorHelpers}
      ${timingHelpers}
      ${k8sHelpers}
      ${metricHelpers}

      # Configuration
      NAMESPACE="${constants.k8s.namespace}"
      DAEMONSET_NAME="${constants.k8s.daemonSetName}"
      MANIFEST_FILE="${manifests.manifestFile}"

      export KERNEL_METRICS="${lib.concatStringsSep " " constants.checks.kernelMetrics}"

      TOTAL_PASSED=0
      TOTAL_FAILED=0
      TOTAL_START=$(time_ms)

      bold "========================================"
      bold "  PCP Kubernetes Quick Test"
      bold "========================================"
      echo ""
      info "Assumes image is already loaded. Use 'pcp-k8s-test' for full test."
      echo ""

      # Check minikube
      if ! minikube_running; then
        error "minikube not running. Please start it first:"
        error ""
        error "  # If minikube is installed on your system:"
        error "  minikube start"
        error ""
        error "  # Or use nix to run minikube:"
        error "  nix shell nixpkgs#minikube -c minikube start"
        exit 1
      fi

      NODE_COUNT=$(get_node_count)
      info "Using minikube cluster with $NODE_COUNT node(s)"
      echo ""

      # Clean up existing deployment
      if kubectl get namespace "$NAMESPACE" &>/dev/null; then
        info "Removing existing deployment..."
        cleanup_namespace "$NAMESPACE" ${toString constants.timeouts.cleanup}
        sleep 2
      fi

      # Deploy
      info "Deploying DaemonSet..."
      kubectl apply -f "$MANIFEST_FILE"

      # Wait for pods
      info "Waiting for pods to be ready..."
      if ! READY_COUNT=$(wait_daemonset_ready "$NAMESPACE" "$DAEMONSET_NAME" ${toString constants.timeouts.podsReady}); then
        error "Pods not ready in time"
        kubectl get pods -n "$NAMESPACE" -o wide
        cleanup_namespace "$NAMESPACE" ${toString constants.timeouts.cleanup}
        exit 1
      fi
      success "$READY_COUNT pods ready"
      echo ""

      PODS=$(get_daemonset_pods "$NAMESPACE" "$DAEMONSET_NAME")

      # Verify process
      info "Verifying processes..."
      for pod in $PODS; do
        node=$(get_pod_node "$NAMESPACE" "$pod")
        if check_process_in_pod "$NAMESPACE" "$pod" "pmcd"; then
          result_pass "pmcd on $node"
          TOTAL_PASSED=$((TOTAL_PASSED + 1))
        else
          result_fail "pmcd on $node"
          TOTAL_FAILED=$((TOTAL_FAILED + 1))
        fi
      done

      # Verify kernel metrics
      echo ""
      info "Verifying kernel metrics..."
      for pod in $PODS; do
        node=$(get_pod_node "$NAMESPACE" "$pod")
        for metric in $KERNEL_METRICS; do
          if check_metric_in_pod "$NAMESPACE" "$pod" "$metric"; then
            result_pass "$metric on $node"
            TOTAL_PASSED=$((TOTAL_PASSED + 1))
          else
            result_fail "$metric on $node"
            TOTAL_FAILED=$((TOTAL_FAILED + 1))
          fi
        done
      done

      # Cleanup
      echo ""
      info "Cleaning up..."
      cleanup_namespace "$NAMESPACE" ${toString constants.timeouts.cleanup}

      TOTAL_ELAPSED=$(elapsed_ms "$TOTAL_START")

      echo ""
      if [[ $TOTAL_FAILED -eq 0 ]]; then
        success "All $TOTAL_PASSED checks passed ($(format_ms "$TOTAL_ELAPSED"))"
      else
        error "$TOTAL_FAILED checks failed"
      fi

      [[ $TOTAL_FAILED -eq 0 ]]
    '';
  };

  # ─── Minikube Start Helper ───────────────────────────────────────────────
  # Starts minikube with optimal settings for PCP testing
  #
  mkMinikubeStart = pkgs.writeShellApplication {
    name = "pcp-minikube-start";
    runtimeInputs = k8sInputs;
    text = ''
      ${colorHelpers}

      bold "========================================"
      bold "  PCP Minikube Setup"
      bold "========================================"
      echo ""

      DRIVER="${constants.minikube.driver}"
      CPUS="${toString constants.minikube.cpus}"
      MEMORY="${toString constants.minikube.memory}"
      DISK="${constants.minikube.diskSize}"

      # Check if minikube is already running
      if minikube status --format='{{.Host}}' 2>/dev/null | grep -q "Running"; then
        warn "Minikube is already running."
        echo ""
        info "Current configuration:"
        minikube config view 2>/dev/null || true
        echo ""
        info "To recreate with optimal settings, run:"
        echo "  minikube delete"
        echo "  nix run .#pcp-minikube-start"
        exit 0
      fi

      info "Starting minikube with settings for PCP testing:"
      echo "  Driver: $DRIVER"
      echo "  CPUs:   $CPUS"
      echo "  Memory: $MEMORY MB"
      echo "  Disk:   $DISK"
      echo ""
      info "Tip: For better I/O performance, use KVM2 driver:"
      echo "  minikube start --driver=kvm2 --cpus=$CPUS --memory=$MEMORY"
      echo "  (requires libvirtd running with default network configured)"
      echo ""

      info "Starting minikube..."
      if minikube start \
        --driver="$DRIVER" \
        --cpus="$CPUS" \
        --memory="$MEMORY" \
        --disk-size="$DISK"; then
        echo ""
        success "Minikube started successfully!"
        echo ""
        info "Run the PCP Kubernetes test:"
        echo "  nix run .#pcp-k8s-test"
      else
        error "Failed to start minikube"
        exit 1
      fi
    '';
  };

in
{
  # Packages output for flake.nix
  packages = {
    pcp-k8s-test = mkFullTest;
    pcp-k8s-test-quick = mkQuickTest;
    pcp-minikube-start = mkMinikubeStart;
  };

  # Apps output for flake.nix
  apps = {
    pcp-k8s-test = {
      type = "app";
      program = "${mkFullTest}/bin/pcp-k8s-test";
    };
    pcp-k8s-test-quick = {
      type = "app";
      program = "${mkQuickTest}/bin/pcp-k8s-test-quick";
    };
    pcp-minikube-start = {
      type = "app";
      program = "${mkMinikubeStart}/bin/pcp-minikube-start";
    };
  };
}
