# nix/lifecycle/pcp-checks.nix
#
# PCP-specific verification functions for MicroVM lifecycle testing.
# Provides service checks, metric verification, and HTTP endpoint testing.
#
{ pkgs, lib }:
let
  constants = import ./constants.nix { };
  mainConstants = import ../constants.nix;
  lifecycleLib = import ./lib.nix { inherit pkgs lib; };

  inherit (lifecycleLib) colorHelpers timingHelpers processHelpers consoleHelpers;
  inherit (lifecycleLib) commonInputs sshInputs;

  # SSH options for connecting to debug VMs
  sshOpts = lib.concatStringsSep " " [
    "-o" "StrictHostKeyChecking=no"
    "-o" "UserKnownHostsFile=/dev/null"
    "-o" "ConnectTimeout=5"
    "-o" "LogLevel=ERROR"
    "-o" "PubkeyAuthentication=no"
  ];

  # Service checking helpers (via SSH)
  sshHelpers = ''
    # Run a command via SSH
    ssh_cmd() {
      local host="$1"
      local port="$2"
      shift 2
      sshpass -p pcp ssh ${sshOpts} -p "$port" "root@$host" "$@" 2>/dev/null
    }

    # Check if a systemd service is active
    check_service() {
      local host="$1"
      local port="$2"
      local service="$3"
      ssh_cmd "$host" "$port" "systemctl is-active $service" | grep -q "^active$"
    }

    # Check if a port is listening (inside VM)
    check_port() {
      local host="$1"
      local port="$2"
      local target_port="$3"
      ssh_cmd "$host" "$port" "ss -tlnp | grep -q :$target_port"
    }

    # Run pminfo and check for output
    check_metric() {
      local host="$1"
      local port="$2"
      local metric="$3"
      ssh_cmd "$host" "$port" "pminfo -f $metric 2>/dev/null | grep -q value"
    }

    # Check HTTP endpoint (inside VM)
    check_http() {
      local host="$1"
      local port="$2"
      local target_port="$3"
      local path="$4"
      ssh_cmd "$host" "$port" "curl -sf http://localhost:$target_port$path >/dev/null"
    }

    # Wait for SSH to be available
    wait_for_ssh() {
      local host="$1"
      local port="$2"
      local timeout="$3"
      local poll_interval=1
      local elapsed=0

      while [[ $elapsed -lt $timeout ]]; do
        if sshpass -p pcp ssh ${sshOpts} -p "$port" "root@$host" true 2>/dev/null; then
          return 0
        fi
        sleep "$poll_interval"
        elapsed=$((elapsed + poll_interval))
      done
      return 1
    }
  '';

in
{
  # Generate a script that verifies PCP services are running
  mkVerifyServicesScript = { variant }:
    let
      variantConfig = constants.variants.${variant};
      portOffset = mainConstants.variantPortOffsets.${variant};
      sshPort = mainConstants.ports.sshForward + portOffset;
      timeout = mainConstants.getTimeout variant "serviceReady";
    in
    pkgs.writeShellApplication {
      name = "pcp-lifecycle-3-verify-services-${variant}";
      runtimeInputs = commonInputs ++ sshInputs ++ [ pkgs.curl ];
      text = ''
        set +e  # Don't exit on first failure

        ${colorHelpers}
        ${timingHelpers}
        ${sshHelpers}

        HOST="localhost"
        SSH_PORT=${toString sshPort}
        TIMEOUT=${toString timeout}
        SERVICES="${lib.concatStringsSep " " variantConfig.services}"

        phase_header "3" "Verify PCP Services" "$TIMEOUT"

        # Wait for SSH first
        info "  Waiting for SSH connectivity..."
        start_time=$(time_ms)
        if ! wait_for_ssh "$HOST" "$SSH_PORT" "$TIMEOUT"; then
          elapsed=$(elapsed_ms "$start_time")
          result_fail "SSH not available within timeout" "$elapsed"
          exit 1
        fi
        ssh_elapsed=$(elapsed_ms "$start_time")
        info "  SSH connected in ''${ssh_elapsed}ms"

        # Check each service
        passed=0
        failed=0

        for service in $SERVICES; do
          start_time=$(time_ms)
          if check_service "$HOST" "$SSH_PORT" "$service"; then
            elapsed=$(elapsed_ms "$start_time")
            result_pass "$service active" "$elapsed"
            passed=$((passed + 1))
          else
            elapsed=$(elapsed_ms "$start_time")
            result_fail "$service not active" "$elapsed"
            failed=$((failed + 1))
          fi
        done

        echo ""
        if [[ $failed -eq 0 ]]; then
          success "  All $passed services verified"
          exit 0
        else
          error "  $failed of $((passed + failed)) services failed"
          exit 1
        fi
      '';
    };

  # Generate a script that verifies PCP metrics are available
  mkVerifyMetricsScript = { variant }:
    let
      variantConfig = constants.variants.${variant};
      portOffset = mainConstants.variantPortOffsets.${variant};
      sshPort = mainConstants.ports.sshForward + portOffset;
      timeout = mainConstants.getTimeout variant "metricsReady";
      metrics = variantConfig.metrics;
      bccMetrics = variantConfig.bccMetrics or [];
    in
    pkgs.writeShellApplication {
      name = "pcp-lifecycle-4-verify-metrics-${variant}";
      runtimeInputs = commonInputs ++ sshInputs ++ [ pkgs.curl ];
      text = ''
        set +e  # Don't exit on first failure

        ${colorHelpers}
        ${timingHelpers}
        ${sshHelpers}

        HOST="localhost"
        SSH_PORT=${toString sshPort}
        TIMEOUT=${toString timeout}
        METRICS="${lib.concatStringsSep " " metrics}"
        ${lib.optionalString (bccMetrics != []) ''BCC_METRICS="${lib.concatStringsSep " " bccMetrics}"''}

        phase_header "4" "Verify PCP Metrics" "$TIMEOUT"

        # Wait for SSH (should already be available from phase 3)
        if ! wait_for_ssh "$HOST" "$SSH_PORT" 10; then
          result_fail "SSH not available" "0"
          exit 1
        fi

        passed=0
        failed=0
        warned=0

        # Check standard metrics
        for metric in $METRICS; do
          start_time=$(time_ms)
          if check_metric "$HOST" "$SSH_PORT" "$metric"; then
            elapsed=$(elapsed_ms "$start_time")
            result_pass "$metric returns data" "$elapsed"
            passed=$((passed + 1))
          else
            elapsed=$(elapsed_ms "$start_time")
            result_fail "$metric not available" "$elapsed"
            failed=$((failed + 1))
          fi
        done

        ${lib.optionalString (bccMetrics != []) ''
        # Check BCC metrics (may take longer, warn instead of fail)
        info "  Checking BCC metrics (may still be compiling)..."
        for metric in $BCC_METRICS; do
          start_time=$(time_ms)
          if check_metric "$HOST" "$SSH_PORT" "$metric"; then
            elapsed=$(elapsed_ms "$start_time")
            result_pass "$metric returns data" "$elapsed"
            passed=$((passed + 1))
          else
            elapsed=$(elapsed_ms "$start_time")
            echo -e "  \033[33mWARN\033[0m: $metric not yet available (''${elapsed}ms)"
            warned=$((warned + 1))
          fi
        done
        ''}

        echo ""
        if [[ $failed -eq 0 ]]; then
          if [[ $warned -gt 0 ]]; then
            warn "  $passed metrics verified, $warned still compiling"
          else
            success "  All $passed metrics verified"
          fi
          exit 0
        else
          error "  $failed metrics failed, $passed passed"
          exit 1
        fi
      '';
    };

  # Generate a script that verifies HTTP endpoints
  mkVerifyHttpScript = { variant }:
    let
      variantConfig = constants.variants.${variant};
      portOffset = mainConstants.variantPortOffsets.${variant};
      sshPort = mainConstants.ports.sshForward + portOffset;
      httpChecks = variantConfig.httpChecks;
    in
    pkgs.writeShellApplication {
      name = "pcp-lifecycle-verify-http-${variant}";
      runtimeInputs = commonInputs ++ sshInputs ++ [ pkgs.curl ];
      text = ''
        set +e  # Don't exit on first failure

        ${colorHelpers}
        ${timingHelpers}
        ${sshHelpers}

        HOST="localhost"
        SSH_PORT=${toString sshPort}

        bold "HTTP Endpoint Verification: ${variant}"
        echo ""

        if ! wait_for_ssh "$HOST" "$SSH_PORT" 10; then
          error "SSH not available"
          exit 1
        fi

        passed=0
        failed=0

        ${lib.concatMapStringsSep "\n" (check: ''
          start_time=$(time_ms)
          if check_http "$HOST" "$SSH_PORT" "${toString check.port}" "${check.path}"; then
            elapsed=$(elapsed_ms "$start_time")
            result_pass "${check.name} HTTP endpoint" "$elapsed"
            passed=$((passed + 1))
          else
            elapsed=$(elapsed_ms "$start_time")
            result_fail "${check.name} HTTP endpoint (port ${toString check.port}${check.path})" "$elapsed"
            failed=$((failed + 1))
          fi
        '') httpChecks}

        echo ""
        if [[ $failed -eq 0 ]]; then
          success "All $passed HTTP endpoints verified"
          exit 0
        else
          error "$failed HTTP endpoints failed"
          exit 1
        fi
      '';
    };

  # Generate a comprehensive service and metric check
  mkFullVerificationScript = { variant }:
    let
      variantConfig = constants.variants.${variant};
      portOffset = mainConstants.variantPortOffsets.${variant};
      sshPort = mainConstants.ports.sshForward + portOffset;
      serviceTimeout = mainConstants.getTimeout variant "serviceReady";
      metricTimeout = mainConstants.getTimeout variant "metricsReady";
    in
    pkgs.writeShellApplication {
      name = "pcp-lifecycle-verify-full-${variant}";
      runtimeInputs = commonInputs ++ sshInputs ++ [ pkgs.curl ];
      text = ''
        set +e

        ${colorHelpers}
        ${timingHelpers}
        ${sshHelpers}

        HOST="localhost"
        SSH_PORT=${toString sshPort}
        SERVICE_TIMEOUT=${toString serviceTimeout}
        METRIC_TIMEOUT=${toString metricTimeout}

        bold "========================================"
        bold "  Full Verification: ${variant}"
        bold "========================================"
        echo ""
        info "Description: ${variantConfig.description}"
        info "SSH Port: $SSH_PORT"
        echo ""

        total_start=$(time_ms)
        total_passed=0
        total_failed=0

        # Wait for SSH
        info "Waiting for SSH (timeout: ''${SERVICE_TIMEOUT}s)..."
        ssh_start=$(time_ms)
        if ! wait_for_ssh "$HOST" "$SSH_PORT" "$SERVICE_TIMEOUT"; then
          result_fail "SSH connectivity" "$(elapsed_ms "$ssh_start")"
          exit 1
        fi
        result_pass "SSH connectivity" "$(elapsed_ms "$ssh_start")"
        total_passed=$((total_passed + 1))

        # Service checks
        echo ""
        bold "--- Service Checks ---"
        ${lib.concatMapStringsSep "\n" (service: ''
          start_time=$(time_ms)
          if check_service "$HOST" "$SSH_PORT" "${service}"; then
            result_pass "Service: ${service}" "$(elapsed_ms "$start_time")"
            total_passed=$((total_passed + 1))
          else
            result_fail "Service: ${service}" "$(elapsed_ms "$start_time")"
            total_failed=$((total_failed + 1))
          fi
        '') variantConfig.services}

        # Metric checks
        echo ""
        bold "--- Metric Checks ---"
        ${lib.concatMapStringsSep "\n" (metric: ''
          start_time=$(time_ms)
          if check_metric "$HOST" "$SSH_PORT" "${metric}"; then
            result_pass "Metric: ${metric}" "$(elapsed_ms "$start_time")"
            total_passed=$((total_passed + 1))
          else
            result_fail "Metric: ${metric}" "$(elapsed_ms "$start_time")"
            total_failed=$((total_failed + 1))
          fi
        '') variantConfig.metrics}

        # HTTP endpoint checks
        echo ""
        bold "--- HTTP Endpoint Checks ---"
        ${lib.concatMapStringsSep "\n" (check: ''
          start_time=$(time_ms)
          if check_http "$HOST" "$SSH_PORT" "${toString check.port}" "${check.path}"; then
            result_pass "HTTP: ${check.name}" "$(elapsed_ms "$start_time")"
            total_passed=$((total_passed + 1))
          else
            result_fail "HTTP: ${check.name}" "$(elapsed_ms "$start_time")"
            total_failed=$((total_failed + 1))
          fi
        '') variantConfig.httpChecks}

        # Summary
        total_elapsed=$(elapsed_ms "$total_start")
        echo ""
        bold "========================================"
        if [[ $total_failed -eq 0 ]]; then
          success "  Result: ALL PASSED ($total_passed checks in $(format_ms "$total_elapsed"))"
        else
          error "  Result: FAILED ($total_failed of $((total_passed + total_failed)) checks)"
        fi
        bold "========================================"

        [[ $total_failed -eq 0 ]]
      '';
    };
}
