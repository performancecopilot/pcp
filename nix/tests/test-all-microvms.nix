# nix/tests/test-all-microvms.nix
#
# Comprehensive test runner for all PCP MicroVM variants.
#
# Features:
# - Polling-based build checking (10s intervals) for slow machine support
# - Sequential builds to leverage Nix caching of shared PCP package
# - Variant-specific port offsets to avoid conflicts
# - Continue on failure, report all results at end
#
# Usage:
#   nix run .#pcp-test-all-microvms
#   nix run .#pcp-test-all-microvms -- --skip-tap    # Skip TAP variants
#   nix run .#pcp-test-all-microvms -- --only=eval   # Test only eval variant
#
{ pkgs, lib }:
let
  constants = import ../constants.nix;
  testLib = import ../test-lib.nix { inherit pkgs lib; };

  # Variant definitions with their properties
  variants = {
    base = {
      name = "pcp-microvm";
      offset = constants.variantPortOffsets.base;
      checks = [ "pmcd" "pmproxy" "pmlogger" ];
      description = "Base PCP (pmcd, pmlogger, pmproxy)";
    };
    base-tap = {
      name = "pcp-microvm-tap";
      offset = constants.variantPortOffsets.base;
      tap = true;
      checks = [ "pmcd" "pmproxy" "pmlogger" ];
      description = "Base PCP with TAP networking";
    };
    eval = {
      name = "pcp-microvm-eval";
      offset = constants.variantPortOffsets.eval;
      checks = [ "pmcd" "pmproxy" "node_exporter" ];
      description = "Eval (+ node_exporter, below, pmie-test)";
    };
    eval-tap = {
      name = "pcp-microvm-eval-tap";
      offset = constants.variantPortOffsets.eval;
      tap = true;
      checks = [ "pmcd" "pmproxy" "node_exporter" ];
      description = "Eval with TAP networking";
    };
    grafana = {
      name = "pcp-microvm-grafana";
      offset = constants.variantPortOffsets.grafana;
      checks = [ "pmcd" "pmproxy" "node_exporter" "grafana" "prometheus" ];
      description = "Grafana (+ Prometheus dashboards)";
    };
    grafana-tap = {
      name = "pcp-microvm-grafana-tap";
      offset = constants.variantPortOffsets.grafana;
      tap = true;
      checks = [ "pmcd" "pmproxy" "node_exporter" "grafana" "prometheus" ];
      description = "Grafana with TAP networking";
    };
    bpf = {
      name = "pcp-microvm-bpf";
      offset = constants.variantPortOffsets.bpf;
      checks = [ "pmcd" "pmproxy" "node_exporter" "bpf" ];
      description = "BPF PMDA (pre-compiled eBPF)";
    };
    bcc = {
      name = "pcp-microvm-bcc";
      offset = constants.variantPortOffsets.bcc;
      checks = [ "pmcd" "pmproxy" "node_exporter" "bcc" ];
      extraTimeout = 120;  # BCC needs longer for eBPF compilation
      description = "BCC PMDA (runtime eBPF, slow startup)";
    };
  };

  sshOpts = lib.concatStringsSep " " testLib.sshOpts;

in
pkgs.writeShellApplication {
  name = "pcp-test-all-microvms";
  runtimeInputs = with pkgs; [
    openssh
    sshpass
    curl
    coreutils
    gnugrep
    procps
    nix
  ];
  text = ''
    set +e  # Don't exit on error - we want to continue and report all results

    # ─── Configuration ─────────────────────────────────────────────────────
    POLL_INTERVAL=${toString constants.test.buildPollSeconds}
    SSH_MAX_ATTEMPTS=${toString constants.test.sshMaxAttempts}
    SSH_RETRY_DELAY=${toString constants.test.sshRetryDelaySeconds}
    BASE_SSH_PORT=${toString constants.ports.sshForward}
    TAP_VM_IP="${constants.network.vmIp}"

    # Service warmup: wait for services to fully start after SSH connects
    # pmlogger takes ~9s, Grafana HTTP takes ~17s after service activation
    SERVICE_WARMUP_SECONDS=15

    # HTTP check retry settings (Grafana/Prometheus may need extra time)
    HTTP_CHECK_RETRIES=3
    HTTP_CHECK_DELAY=5

    # Guest-side ports (used in checks via SSH)
    GUEST_PMCD_PORT=${toString constants.ports.pmcd}
    GUEST_PMPROXY_PORT=${toString constants.ports.pmproxy}
    GUEST_NODE_EXPORTER_PORT=${toString constants.ports.nodeExporter}
    GUEST_GRAFANA_PORT=${toString constants.ports.grafana}
    GUEST_PROMETHEUS_PORT=${toString constants.ports.prometheus}

    # Results tracking
    declare -A RESULTS
    declare -A DURATIONS
    TOTAL_PASSED=0
    TOTAL_FAILED=0
    TOTAL_SKIPPED=0

    # CLI options
    SKIP_TAP=false
    ONLY_VARIANT=""

    # ─── Argument Parsing ──────────────────────────────────────────────────
    while [[ $# -gt 0 ]]; do
      case "$1" in
        --skip-tap)
          SKIP_TAP=true
          shift
          ;;
        --only=*)
          ONLY_VARIANT="''${1#--only=}"
          shift
          ;;
        --help|-h)
          echo "Usage: pcp-test-all-microvms [OPTIONS]"
          echo ""
          echo "Options:"
          echo "  --skip-tap       Skip TAP networking variants"
          echo "  --only=VARIANT   Test only specified variant (base, eval, grafana, bpf, bcc)"
          echo "  --help, -h       Show this help"
          echo ""
          echo "Variants:"
          echo "  base, base-tap, eval, eval-tap, grafana, grafana-tap, bpf, bcc"
          exit 0
          ;;
        *)
          echo "Unknown option: $1"
          exit 1
          ;;
      esac
    done

    # ─── Helper Functions ──────────────────────────────────────────────────

    log() {
      echo "[$(date '+%H:%M:%S')] $*"
    }

    log_section() {
      echo ""
      echo "════════════════════════════════════════════════════════════════════"
      echo "  $*"
      echo "════════════════════════════════════════════════════════════════════"
    }

    # Poll for build completion
    wait_for_build() {
      local variant="$1"
      local result_link="$2"
      local start_time
      start_time=$(date +%s)

      log "Building $variant..."

      # Start build in background
      nix build ".#$variant" -o "$result_link" 2>&1 &
      local build_pid=$!

      # Poll for completion
      while kill -0 "$build_pid" 2>/dev/null; do
        local elapsed=$(( $(date +%s) - start_time ))
        log "  still building... (''${elapsed}s elapsed)"
        sleep "$POLL_INTERVAL"
      done

      # Check if build succeeded
      wait "$build_pid"
      local exit_code=$?

      local duration=$(( $(date +%s) - start_time ))
      if [[ $exit_code -eq 0 ]] && [[ -L "$result_link" ]]; then
        log "Build complete (''${duration}s)"
        return 0
      else
        log "Build FAILED (exit code: $exit_code)"
        return 1
      fi
    }

    # Wait for SSH to become available
    wait_for_ssh() {
      local host="$1"
      local port="$2"
      local max_attempts="''${3:-$SSH_MAX_ATTEMPTS}"
      local attempt=0

      log "Waiting for SSH on $host:$port..."

      while ! sshpass -p pcp ssh ${sshOpts} -p "$port" "root@$host" true 2>/dev/null; do
        attempt=$((attempt + 1))
        if [[ $attempt -ge $max_attempts ]]; then
          log "SSH not available after $max_attempts attempts"
          return 1
        fi
        echo -n "."
        sleep "$SSH_RETRY_DELAY"
      done
      echo ""
      log "SSH connected"
      return 0
    }

    # Run a check command via SSH
    run_ssh_check() {
      local host="$1"
      local port="$2"
      local desc="$3"
      shift 3
      local cmd="$*"

      echo -n "  CHECK: $desc ... "
      if sshpass -p pcp ssh ${sshOpts} -p "$port" "root@$host" "$cmd" >/dev/null 2>&1; then
        echo "OK"
        return 0
      else
        echo "FAIL"
        return 1
      fi
    }

    # Check if a service is running
    check_service() {
      local host="$1"
      local port="$2"
      local service="$3"
      run_ssh_check "$host" "$port" "service $service active" "systemctl is-active $service"
    }

    # Check if a port is listening
    check_port() {
      local host="$1"
      local port="$2"
      local target_port="$3"
      run_ssh_check "$host" "$port" "port $target_port listening" "ss -tlnp | grep -q :$target_port"
    }

    # Check if pminfo returns metrics
    check_pminfo() {
      local host="$1"
      local port="$2"
      run_ssh_check "$host" "$port" "pminfo kernel.all.load" "pminfo -f kernel.all.load"
    }

    # Check Grafana HTTP endpoint (with retry - Grafana needs extra startup time)
    check_grafana_http() {
      local host="$1"
      local port="$2"
      local attempt=0

      echo -n "  CHECK: Grafana HTTP on port $GUEST_GRAFANA_PORT ... "
      while [[ $attempt -lt $HTTP_CHECK_RETRIES ]]; do
        if sshpass -p pcp ssh ${sshOpts} -p "$port" "root@$host" \
           "curl -sf http://localhost:$GUEST_GRAFANA_PORT/api/health" >/dev/null 2>&1; then
          echo "OK"
          return 0
        fi
        attempt=$((attempt + 1))
        if [[ $attempt -lt $HTTP_CHECK_RETRIES ]]; then
          echo -n "(retry $attempt) "
          sleep "$HTTP_CHECK_DELAY"
        fi
      done
      echo "FAIL"
      return 1
    }

    # Check Prometheus HTTP endpoint (with retry)
    check_prometheus_http() {
      local host="$1"
      local port="$2"
      local attempt=0

      echo -n "  CHECK: Prometheus HTTP on port $GUEST_PROMETHEUS_PORT ... "
      while [[ $attempt -lt $HTTP_CHECK_RETRIES ]]; do
        if sshpass -p pcp ssh ${sshOpts} -p "$port" "root@$host" \
           "curl -sf http://localhost:$GUEST_PROMETHEUS_PORT/-/ready" >/dev/null 2>&1; then
          echo "OK"
          return 0
        fi
        attempt=$((attempt + 1))
        if [[ $attempt -lt $HTTP_CHECK_RETRIES ]]; then
          echo -n "(retry $attempt) "
          sleep "$HTTP_CHECK_DELAY"
        fi
      done
      echo "FAIL"
      return 1
    }

    # Check BPF metrics
    check_bpf_metrics() {
      local host="$1"
      local port="$2"
      run_ssh_check "$host" "$port" "BPF metrics available" \
        "pminfo bpf 2>/dev/null | grep -q bpf"
    }

    # Check BCC metrics
    check_bcc_metrics() {
      local host="$1"
      local port="$2"
      run_ssh_check "$host" "$port" "BCC metrics available" \
        "pminfo bcc 2>/dev/null | grep -q bcc"
    }

    # Stop all PCP MicroVMs
    stop_all_vms() {
      log "Stopping any running PCP MicroVMs..."
      pkill -f 'microvm@pcp-(vm|eval-vm|grafana-vm|bpf-vm|bcc-vm)' 2>/dev/null || true
      sleep 2
      # Force kill if still running
      pkill -9 -f 'microvm@pcp-(vm|eval-vm|grafana-vm|bpf-vm|bcc-vm)' 2>/dev/null || true
    }

    # ─── Test Runner for a Single Variant ──────────────────────────────────

    test_variant() {
      local key="$1"
      local name="$2"
      local offset="$3"
      local is_tap="$4"
      local checks="$5"
      local extra_timeout="''${6:-0}"
      local description="$7"

      local start_time
      start_time=$(date +%s)

      log_section "Testing: $name"
      log "Description: $description"
      log "Port offset: $offset"

      # Calculate ports (offset applied to host-side forwarded ports)
      local ssh_port=$((BASE_SSH_PORT + offset))
      # Note: Guest-side ports are fixed (from constants), host-side are offset
      # These are logged for debugging but checks use the constant guest ports

      # For TAP, use direct IP
      local ssh_host="localhost"
      if [[ "$is_tap" == "true" ]]; then
        ssh_host="$TAP_VM_IP"
        ssh_port=22
      fi

      local result_link="result-test-$key"
      local checks_passed=0
      local checks_failed=0

      # Phase 1: Build
      log ""
      log "Phase 1: Build"
      if ! wait_for_build "$name" "$result_link"; then
        RESULTS[$key]="BUILD_FAILED"
        DURATIONS[$key]=$(( $(date +%s) - start_time ))
        return 1
      fi

      # Phase 2: Start VM
      log ""
      log "Phase 2: Start VM"
      log "Starting $result_link/bin/microvm-run..."
      "$result_link/bin/microvm-run" &
      local vm_pid=$!
      sleep 3  # Give VM a moment to initialize

      # Check VM process is running
      if ! kill -0 "$vm_pid" 2>/dev/null; then
        log "VM process died immediately"
        RESULTS[$key]="VM_START_FAILED"
        DURATIONS[$key]=$(( $(date +%s) - start_time ))
        return 1
      fi

      # Phase 3: Wait for SSH
      log ""
      log "Phase 3: SSH connectivity"
      local max_attempts=$SSH_MAX_ATTEMPTS
      if [[ $extra_timeout -gt 0 ]]; then
        max_attempts=$((max_attempts + extra_timeout / SSH_RETRY_DELAY))
        log "(Extended timeout for this variant: +''${extra_timeout}s)"
      fi

      if ! wait_for_ssh "$ssh_host" "$ssh_port" "$max_attempts"; then
        log "SSH connectivity failed"
        stop_all_vms
        rm -f "$result_link"
        RESULTS[$key]="SSH_FAILED"
        DURATIONS[$key]=$(( $(date +%s) - start_time ))
        return 1
      fi

      # Wait for services to fully start (pmlogger takes ~9s, Grafana ~17s)
      # BCC variants need extra time for eBPF compilation (30-60s)
      local warmup_time=$SERVICE_WARMUP_SECONDS
      if [[ $extra_timeout -gt 0 ]]; then
        warmup_time=$((SERVICE_WARMUP_SECONDS + extra_timeout))
        log "Waiting ''${warmup_time}s for services (includes BCC compilation time)..."
      else
        log "Waiting ''${warmup_time}s for services to start..."
      fi
      sleep "$warmup_time"

      # Phase 4: Run checks
      log ""
      log "Phase 4: Service checks"

      # Always check pmcd and basic metrics
      if check_service "$ssh_host" "$ssh_port" "pmcd"; then
        checks_passed=$((checks_passed + 1))
      else
        checks_failed=$((checks_failed + 1))
      fi

      if check_port "$ssh_host" "$ssh_port" "$GUEST_PMCD_PORT"; then
        checks_passed=$((checks_passed + 1))
      else
        checks_failed=$((checks_failed + 1))
      fi

      if check_pminfo "$ssh_host" "$ssh_port"; then
        checks_passed=$((checks_passed + 1))
      else
        checks_failed=$((checks_failed + 1))
      fi

      # Check pmproxy if in checks list
      if [[ "$checks" == *"pmproxy"* ]]; then
        if check_service "$ssh_host" "$ssh_port" "pmproxy"; then
          checks_passed=$((checks_passed + 1))
        else
          checks_failed=$((checks_failed + 1))
        fi
        if check_port "$ssh_host" "$ssh_port" "$GUEST_PMPROXY_PORT"; then
          checks_passed=$((checks_passed + 1))
        else
          checks_failed=$((checks_failed + 1))
        fi
      fi

      # Check pmlogger if in checks list
      if [[ "$checks" == *"pmlogger"* ]]; then
        if check_service "$ssh_host" "$ssh_port" "pmlogger"; then
          checks_passed=$((checks_passed + 1))
        else
          checks_failed=$((checks_failed + 1))
        fi
      fi

      # Check node_exporter if in checks list
      if [[ "$checks" == *"node_exporter"* ]]; then
        if check_service "$ssh_host" "$ssh_port" "prometheus-node-exporter"; then
          checks_passed=$((checks_passed + 1))
        else
          checks_failed=$((checks_failed + 1))
        fi
        if check_port "$ssh_host" "$ssh_port" "$GUEST_NODE_EXPORTER_PORT"; then
          checks_passed=$((checks_passed + 1))
        else
          checks_failed=$((checks_failed + 1))
        fi
      fi

      # Check Grafana if in checks list
      if [[ "$checks" == *"grafana"* ]]; then
        if check_service "$ssh_host" "$ssh_port" "grafana"; then
          checks_passed=$((checks_passed + 1))
        else
          checks_failed=$((checks_failed + 1))
        fi
        if check_grafana_http "$ssh_host" "$ssh_port"; then
          checks_passed=$((checks_passed + 1))
        else
          checks_failed=$((checks_failed + 1))
        fi
      fi

      # Check Prometheus if in checks list
      if [[ "$checks" == *"prometheus"* ]]; then
        if check_service "$ssh_host" "$ssh_port" "prometheus"; then
          checks_passed=$((checks_passed + 1))
        else
          checks_failed=$((checks_failed + 1))
        fi
        if check_prometheus_http "$ssh_host" "$ssh_port"; then
          checks_passed=$((checks_passed + 1))
        else
          checks_failed=$((checks_failed + 1))
        fi
      fi

      # Check BPF if in checks list
      if [[ "$checks" == *"bpf"* ]]; then
        if check_bpf_metrics "$ssh_host" "$ssh_port"; then
          checks_passed=$((checks_passed + 1))
        else
          checks_failed=$((checks_failed + 1))
        fi
      fi

      # Check BCC if in checks list
      if [[ "$checks" == *"bcc"* ]]; then
        if check_bcc_metrics "$ssh_host" "$ssh_port"; then
          checks_passed=$((checks_passed + 1))
        else
          checks_failed=$((checks_failed + 1))
        fi
      fi

      # Phase 5: Cleanup
      log ""
      log "Phase 5: Cleanup"
      stop_all_vms
      rm -f "$result_link"

      # Record result
      local duration=$(( $(date +%s) - start_time ))
      DURATIONS[$key]=$duration

      if [[ $checks_failed -eq 0 ]]; then
        RESULTS[$key]="PASSED ($checks_passed checks)"
        TOTAL_PASSED=$((TOTAL_PASSED + 1))
        log "Result: PASSED ($checks_passed checks in ''${duration}s)"
        return 0
      else
        RESULTS[$key]="FAILED ($checks_failed/$((checks_passed + checks_failed)) failed)"
        TOTAL_FAILED=$((TOTAL_FAILED + 1))
        log "Result: FAILED ($checks_failed checks failed in ''${duration}s)"
        return 1
      fi
    }

    # ─── Main ──────────────────────────────────────────────────────────────

    log_section "PCP MicroVM Test Suite"
    log "Starting comprehensive MicroVM tests"
    log "Build poll interval: ''${POLL_INTERVAL}s"
    log "Skip TAP variants: $SKIP_TAP"
    [[ -n "$ONLY_VARIANT" ]] && log "Only testing: $ONLY_VARIANT"

    # Ensure clean state
    stop_all_vms

    # Define test order
    VARIANT_ORDER=(base base-tap eval eval-tap grafana grafana-tap bpf bcc)

    for key in "''${VARIANT_ORDER[@]}"; do
      # Skip if --only specified and doesn't match
      if [[ -n "$ONLY_VARIANT" ]] && [[ "$key" != "$ONLY_VARIANT" ]] && [[ "$key" != "$ONLY_VARIANT-tap" ]]; then
        continue
      fi

      # Skip TAP variants if requested
      if [[ "$SKIP_TAP" == "true" ]] && [[ "$key" == *"-tap" ]]; then
        RESULTS[$key]="SKIPPED"
        DURATIONS[$key]=0
        TOTAL_SKIPPED=$((TOTAL_SKIPPED + 1))
        continue
      fi

      # Get variant properties
      case "$key" in
        base)
          test_variant "$key" "pcp-microvm" "${toString constants.variantPortOffsets.base}" "false" \
            "pmcd pmproxy pmlogger" "0" "Base PCP (pmcd, pmlogger, pmproxy)"
          ;;
        base-tap)
          test_variant "$key" "pcp-microvm-tap" "${toString constants.variantPortOffsets.base}" "true" \
            "pmcd pmproxy pmlogger" "0" "Base PCP with TAP networking"
          ;;
        eval)
          test_variant "$key" "pcp-microvm-eval" "${toString constants.variantPortOffsets.eval}" "false" \
            "pmcd pmproxy node_exporter" "0" "Eval (+ node_exporter, below, pmie-test)"
          ;;
        eval-tap)
          test_variant "$key" "pcp-microvm-eval-tap" "${toString constants.variantPortOffsets.eval}" "true" \
            "pmcd pmproxy node_exporter" "0" "Eval with TAP networking"
          ;;
        grafana)
          test_variant "$key" "pcp-microvm-grafana" "${toString constants.variantPortOffsets.grafana}" "false" \
            "pmcd pmproxy node_exporter grafana prometheus" "0" "Grafana (+ Prometheus dashboards)"
          ;;
        grafana-tap)
          test_variant "$key" "pcp-microvm-grafana-tap" "${toString constants.variantPortOffsets.grafana}" "true" \
            "pmcd pmproxy node_exporter grafana prometheus" "0" "Grafana with TAP networking"
          ;;
        bpf)
          test_variant "$key" "pcp-microvm-bpf" "${toString constants.variantPortOffsets.bpf}" "false" \
            "pmcd pmproxy node_exporter bpf" "0" "BPF PMDA (pre-compiled eBPF)"
          ;;
        bcc)
          test_variant "$key" "pcp-microvm-bcc" "${toString constants.variantPortOffsets.bcc}" "false" \
            "pmcd pmproxy node_exporter bcc" "120" "BCC PMDA (runtime eBPF, slow startup)"
          ;;
      esac
    done

    # ─── Summary ───────────────────────────────────────────────────────────

    log_section "Test Summary"

    echo ""
    printf "%-15s %-40s %10s\n" "VARIANT" "RESULT" "DURATION"
    printf "%-15s %-40s %10s\n" "───────" "──────" "────────"

    for key in "''${VARIANT_ORDER[@]}"; do
      if [[ -n "''${RESULTS[$key]:-}" ]]; then
        printf "%-15s %-40s %10s\n" "$key" "''${RESULTS[$key]}" "''${DURATIONS[$key]}s"
      fi
    done

    echo ""
    echo "────────────────────────────────────────────────────────────────────"
    echo "Total: $TOTAL_PASSED passed, $TOTAL_FAILED failed, $TOTAL_SKIPPED skipped"
    echo "────────────────────────────────────────────────────────────────────"

    if [[ $TOTAL_FAILED -gt 0 ]]; then
      exit 1
    fi
    exit 0
  '';
}
