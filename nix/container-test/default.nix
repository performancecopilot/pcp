# nix/container-test/default.nix
#
# Entry point for PCP OCI container lifecycle testing.
# Generates lifecycle test scripts for the PCP container image.
#
# Usage in flake.nix:
#   containerTest = import ./nix/container-test { inherit pkgs lib pcp; };
#
# Generated outputs:
#   containerTest.packages.pcp-container-test  - Full lifecycle test
#   containerTest.apps.pcp-container-test      - App entry point
#
{ pkgs, lib, pcp, containerInputsHash }:
let
  constants = import ./constants.nix { };
  mainConstants = import ../constants.nix;
  helpers = import ./lib.nix { inherit pkgs lib; };

  inherit (helpers)
    colorHelpers timingHelpers containerHelpers
    portHelpers processHelpers metricHelpers
    commonInputs containerInputs;

  # PCP tools for metric verification (use local package, not nixpkgs)
  pcpInputs = [ pcp ];

  # PCP_CONF path for pminfo to find its configuration
  pcpConfPath = "${pcp}/share/pcp/etc/pcp.conf";

  # ─── Full Lifecycle Test Script ─────────────────────────────────────────
  # Tests the complete container lifecycle:
  # Build -> Load -> Start -> Verify -> Shutdown -> Cleanup
  #
  mkFullTest = pkgs.writeShellApplication {
    name = "pcp-container-test";
    runtimeInputs = commonInputs ++ containerInputs ++ pcpInputs;
    text = ''
      set +e  # Don't exit on first failure

      # Set PCP_CONF so pminfo can find its configuration
      export PCP_CONF="${pcpConfPath}"

      ${colorHelpers}
      ${timingHelpers}
      ${containerHelpers}
      ${portHelpers}
      ${processHelpers}
      ${metricHelpers}

      # Configuration
      CONTAINER_NAME="${constants.container.name}"
      IMAGE="${constants.container.imageName}:${constants.container.imageTag}"
      RESULT_LINK="result-container"

      # Port configuration
      PMCD_PORT=${toString mainConstants.ports.pmcd}
      PMPROXY_PORT=${toString mainConstants.ports.pmproxy}

      # Metrics to verify
      METRICS="${lib.concatStringsSep " " constants.checks.metrics}"
      KERNEL_METRICS="${lib.concatStringsSep " " constants.checks.kernelMetrics}"
      BPF_METRICS="${lib.concatStringsSep " " constants.checks.bpfMetrics}"

      # Timing tracking
      declare -A PHASE_TIMES
      TOTAL_START=$(time_ms)

      # Results tracking
      TOTAL_PASSED=0
      TOTAL_FAILED=0
      TOTAL_SKIPPED=0

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
        if [[ -n "$CONTAINER_RUNTIME" ]]; then
          container_remove "$CONTAINER_NAME"
        fi
        rm -f "$RESULT_LINK"
      }

      # Header
      bold "========================================"
      bold "  PCP Container Lifecycle Test"
      bold "========================================"
      echo ""

      # Detect container runtime
      detect_runtime
      echo ""

      # ─── Phase 0: Build Image ─────────────────────────────────────────────
      phase_header "0" "Build Image" "${toString constants.timeouts.build}"
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

      # ─── Phase 1: Load Image ──────────────────────────────────────────────
      phase_header "1" "Load Image" "${toString constants.timeouts.load}"
      start_time=$(time_ms)

      # Fast cache check using Nix inputs hash label
      EXPECTED_HASH="${containerInputsHash}"
      LOADED_HASH=""

      # Check if image exists and get its inputs hash label
      if $CONTAINER_RUNTIME image inspect "$IMAGE" &>/dev/null; then
        LOADED_HASH=$($CONTAINER_RUNTIME inspect "$IMAGE" --format '{{index .Config.Labels "nix.inputs.hash"}}' 2>/dev/null || echo "")
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
        info "  Loading image into $CONTAINER_RUNTIME..."
        if $CONTAINER_RUNTIME load < "$RESULT_LINK" 2>&1 | while read -r line; do
          echo "    $line"
        done; then
          elapsed=$(elapsed_ms "$start_time")
          result_pass "Image loaded" "$elapsed"
          record_result "load" "true" "$elapsed"
        else
          elapsed=$(elapsed_ms "$start_time")
          result_fail "Failed to load image" "$elapsed"
          record_result "load" "false" "$elapsed"
          cleanup
          exit 1
        fi
      fi

      # ─── Phase 2: Start Container ─────────────────────────────────────────
      phase_header "2" "Start Container" "${toString constants.timeouts.start}"
      start_time=$(time_ms)

      # Remove existing container if present
      if container_exists "$CONTAINER_NAME"; then
        info "  Removing existing container..."
        container_remove "$CONTAINER_NAME"
        sleep 1
      fi

      info "  Starting container with port mappings (privileged + root for BPF)..."
      if $CONTAINER_RUNTIME run -d \
          --name "$CONTAINER_NAME" \
          --privileged \
          --user root \
          -p "$PMCD_PORT:$PMCD_PORT" \
          -p "$PMPROXY_PORT:$PMPROXY_PORT" \
          "$IMAGE" 2>&1; then
        elapsed=$(elapsed_ms "$start_time")
        result_pass "Container started" "$elapsed"
        record_result "start" "true" "$elapsed"
      else
        elapsed=$(elapsed_ms "$start_time")
        result_fail "Failed to start container" "$elapsed"
        record_result "start" "false" "$elapsed"
        cleanup
        exit 1
      fi

      # ─── Phase 3: Verify Process ──────────────────────────────────────────
      phase_header "3" "Verify Process" "${toString constants.timeouts.ready}"
      start_time=$(time_ms)

      # Give pmcd time to start
      sleep 2

      ${lib.concatMapStringsSep "\n" (proc: ''
        proc_start=$(time_ms)
        if wait_for_process_in_container "${proc}" ${toString constants.timeouts.ready}; then
          result_pass "${proc} running" "$(elapsed_ms "$proc_start")"
          TOTAL_PASSED=$((TOTAL_PASSED + 1))
        else
          result_fail "${proc} not running" "$(elapsed_ms "$proc_start")"
          TOTAL_FAILED=$((TOTAL_FAILED + 1))
        fi
      '') constants.checks.processes}

      elapsed=$(elapsed_ms "$start_time")
      record_result "process" "true" "$elapsed"

      # ─── Phase 4: Verify Ports ────────────────────────────────────────────
      phase_header "4" "Verify Ports" "${toString constants.timeouts.ready}"
      start_time=$(time_ms)

      port_passed=0
      port_failed=0

      for port in $PMCD_PORT; do  # Only pmcd runs in container
        port_start=$(time_ms)
        if wait_for_port "$port" ${toString constants.timeouts.ready}; then
          result_pass "Port $port listening" "$(elapsed_ms "$port_start")"
          port_passed=$((port_passed + 1))
        else
          result_fail "Port $port not listening" "$(elapsed_ms "$port_start")"
          port_failed=$((port_failed + 1))
        fi
      done

      elapsed=$(elapsed_ms "$start_time")
      if [[ $port_failed -eq 0 ]]; then
        record_result "ports" "true" "$elapsed"
      else
        record_result "ports" "false" "$elapsed"
      fi

      # ─── Phase 5: Verify Metrics ──────────────────────────────────────────
      phase_header "5" "Verify Metrics" "${toString constants.timeouts.ready}"
      start_time=$(time_ms)

      metric_passed=0
      metric_failed=0

      for metric in $METRICS; do
        met_start=$(time_ms)
        if check_metric "$metric"; then
          result_pass "$metric" "$(elapsed_ms "$met_start")"
          metric_passed=$((metric_passed + 1))
        else
          result_fail "$metric not available" "$(elapsed_ms "$met_start")"
          metric_failed=$((metric_failed + 1))
        fi
      done

      elapsed=$(elapsed_ms "$start_time")
      if [[ $metric_failed -eq 0 ]]; then
        record_result "metrics" "true" "$elapsed"
      else
        record_result "metrics" "false" "$elapsed"
      fi

      # ─── Phase 5b: Verify Kernel Metrics ─────────────────────────────────
      phase_header "5b" "Verify Kernel Metrics" "${toString constants.timeouts.ready}"
      start_time=$(time_ms)

      kernel_passed=0
      kernel_failed=0

      for metric in $KERNEL_METRICS; do
        met_start=$(time_ms)
        if check_metric "$metric"; then
          result_pass "$metric" "$(elapsed_ms "$met_start")"
          kernel_passed=$((kernel_passed + 1))
        else
          result_fail "$metric not available" "$(elapsed_ms "$met_start")"
          kernel_failed=$((kernel_failed + 1))
        fi
      done

      elapsed=$(elapsed_ms "$start_time")
      if [[ $kernel_failed -eq 0 ]]; then
        record_result "kernel_metrics" "true" "$elapsed"
      else
        record_result "kernel_metrics" "false" "$elapsed"
      fi

      # ─── Phase 5c: Verify BPF Metrics ────────────────────────────────────
      phase_header "5c" "Verify BPF Metrics" "${toString constants.timeouts.ready}"
      start_time=$(time_ms)

      BPF_AVAILABLE=true

      # Check if BPF PMDA is loaded
      if ! check_metric "bpf"; then
        warn "  BPF PMDA not loaded - BPF metrics will be skipped"
        BPF_AVAILABLE=false
      fi

      if $BPF_AVAILABLE; then
        bpf_passed=0
        bpf_failed=0

        for metric in $BPF_METRICS; do
          met_start=$(time_ms)
          # BPF histogram metrics need time to populate, retry a few times
          retry=0
          max_retries=6
          metric_ok=false

          while [[ $retry -lt $max_retries ]]; do
            if pminfo -h "$CONTAINER_IP" -f "$metric" 2>/dev/null | grep -qE '(inst|value)'; then
              metric_ok=true
              break
            fi
            sleep 5
            retry=$((retry + 1))
          done

          if $metric_ok; then
            result_pass "$metric" "$(elapsed_ms "$met_start")"
            bpf_passed=$((bpf_passed + 1))
          else
            result_fail "$metric not available" "$(elapsed_ms "$met_start")"
            bpf_failed=$((bpf_failed + 1))
          fi
        done

        elapsed=$(elapsed_ms "$start_time")
        if [[ $bpf_failed -eq 0 ]]; then
          record_result "bpf_metrics" "true" "$elapsed"
        else
          record_result "bpf_metrics" "false" "$elapsed"
        fi
      else
        for metric in $BPF_METRICS; do
          result_skip "$metric (BPF unavailable)"
          TOTAL_SKIPPED=$((TOTAL_SKIPPED + 1))
        done
        elapsed=$(elapsed_ms "$start_time")
        record_result "bpf_metrics" "skip" "$elapsed"
      fi

      # ─── Phase 6: Shutdown ────────────────────────────────────────────────
      phase_header "6" "Shutdown" "${toString constants.timeouts.shutdown}"
      start_time=$(time_ms)

      info "  Stopping container..."
      if container_stop "$CONTAINER_NAME" ${toString constants.timeouts.shutdown}; then
        elapsed=$(elapsed_ms "$start_time")
        result_pass "Clean shutdown" "$elapsed"
        record_result "shutdown" "true" "$elapsed"
      else
        elapsed=$(elapsed_ms "$start_time")
        warn "  Graceful shutdown failed, forcing..."
        container_kill "$CONTAINER_NAME"
        result_warn "Forced kill" "$elapsed"
        record_result "shutdown" "true" "$elapsed"
      fi

      # ─── Phase 7: Cleanup ─────────────────────────────────────────────────
      phase_header "7" "Cleanup" "${toString constants.timeouts.cleanup}"
      start_time=$(time_ms)

      container_remove "$CONTAINER_NAME"
      rm -f "$RESULT_LINK"

      elapsed=$(elapsed_ms "$start_time")
      result_pass "Container removed" "$elapsed"
      record_result "cleanup" "true" "$elapsed"

      # ─── Summary ──────────────────────────────────────────────────────────
      TOTAL_ELAPSED=$(elapsed_ms "$TOTAL_START")

      echo ""
      bold "  Timing Summary"
      echo "  $(printf '─%.0s' {1..37})"
      printf "  %-20s %10s\n" "Phase" "Time (ms)"
      echo "  $(printf '─%.0s' {1..37})"
      for phase in build load start process ports metrics kernel_metrics bpf_metrics shutdown cleanup; do
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

  # ─── Quick Test Script ──────────────────────────────────────────────────
  # A faster test that skips the build phase (assumes image already exists)
  #
  mkQuickTest = pkgs.writeShellApplication {
    name = "pcp-container-test-quick";
    runtimeInputs = commonInputs ++ containerInputs ++ pcpInputs;
    text = ''
      set +e

      # Set PCP_CONF so pminfo can find its configuration
      export PCP_CONF="${pcpConfPath}"

      ${colorHelpers}
      ${timingHelpers}
      ${containerHelpers}
      ${portHelpers}
      ${processHelpers}
      ${metricHelpers}

      # Configuration
      CONTAINER_NAME="${constants.container.name}"
      IMAGE="${constants.container.imageName}:${constants.container.imageTag}"

      PMCD_PORT=${toString mainConstants.ports.pmcd}
      PMPROXY_PORT=${toString mainConstants.ports.pmproxy}
      METRICS="${lib.concatStringsSep " " constants.checks.metrics}"

      TOTAL_PASSED=0
      TOTAL_FAILED=0

      bold "========================================"
      bold "  PCP Container Quick Test"
      bold "========================================"
      echo ""
      info "Assumes image is already loaded. Use 'pcp-container-test' for full test."
      echo ""

      detect_runtime

      # Cleanup existing
      if container_exists "$CONTAINER_NAME"; then
        container_remove "$CONTAINER_NAME"
        sleep 1
      fi

      # Start
      info "Starting container..."
      $CONTAINER_RUNTIME run -d \
        --name "$CONTAINER_NAME" \
        -p "$PMCD_PORT:$PMCD_PORT" \
        -p "$PMPROXY_PORT:$PMPROXY_PORT" \
        "$IMAGE"

      sleep 3

      # Verify process
      ${lib.concatMapStringsSep "\n" (proc: ''
        if check_process_in_container "${proc}"; then
          result_pass "${proc} running"
          TOTAL_PASSED=$((TOTAL_PASSED + 1))
        else
          result_fail "${proc} not running"
          TOTAL_FAILED=$((TOTAL_FAILED + 1))
        fi
      '') constants.checks.processes}

      # Verify ports
      for port in $PMCD_PORT; do  # Only pmcd runs in container
        if port_is_open "$port"; then
          result_pass "Port $port listening"
          TOTAL_PASSED=$((TOTAL_PASSED + 1))
        else
          result_fail "Port $port not listening"
          TOTAL_FAILED=$((TOTAL_FAILED + 1))
        fi
      done

      # Verify metrics
      for metric in $METRICS; do
        if check_metric "$metric"; then
          result_pass "$metric"
          TOTAL_PASSED=$((TOTAL_PASSED + 1))
        else
          result_fail "$metric"
          TOTAL_FAILED=$((TOTAL_FAILED + 1))
        fi
      done

      # Cleanup
      container_remove "$CONTAINER_NAME"

      echo ""
      if [[ $TOTAL_FAILED -eq 0 ]]; then
        success "All $TOTAL_PASSED checks passed"
      else
        error "$TOTAL_FAILED checks failed"
      fi

      [[ $TOTAL_FAILED -eq 0 ]]
    '';
  };

in
{
  # Packages output for flake.nix
  packages = {
    pcp-container-test = mkFullTest;
    pcp-container-test-quick = mkQuickTest;
  };

  # Apps output for flake.nix
  apps = {
    pcp-container-test = {
      type = "app";
      program = "${mkFullTest}/bin/pcp-container-test";
    };
    pcp-container-test-quick = {
      type = "app";
      program = "${mkQuickTest}/bin/pcp-container-test-quick";
    };
  };
}
