# nix/lifecycle/default.nix
#
# Entry point for PCP MicroVM lifecycle testing.
# Generates lifecycle test scripts for all MicroVM variants.
#
# Usage in flake.nix:
#   lifecycle = import ./nix/lifecycle { inherit pkgs lib; };
#
# Generated outputs:
#   lifecycle.scripts.<variant>.<phase>  - Individual phase scripts
#   lifecycle.tests.<variant>            - Full lifecycle test for variant
#   lifecycle.tests.all                  - Test all variants sequentially
#
{ pkgs, lib }:
let
  constants = import ./constants.nix { };
  mainConstants = import ../constants.nix;
  lifecycleLib = import ./lib.nix { inherit pkgs lib; };
  pcpChecks = import ./pcp-checks.nix { inherit pkgs lib; };

  inherit (lifecycleLib) colorHelpers timingHelpers processHelpers consoleHelpers;
  inherit (lifecycleLib) commonInputs sshInputs;

  # All variant names (user-mode networking)
  # NOTE: BCC is deprecated and no longer supported. Use BPF PMDA instead.
  # BCC used runtime eBPF compilation which is slower and less reliable than
  # the pre-compiled BPF PMDA CO-RE approach.
  variantNames = [ "base" "eval" "grafana" "bpf" ];

  # TAP networking variants (require host network setup first)
  # These use direct IP access instead of port forwarding.
  # Only one TAP VM can run at a time (they share the same IP).
  tapVariantNames = [ "grafana-tap" ];

  # All variant names including TAP variants
  allVariantNames = variantNames ++ tapVariantNames;

  # Parse a variant name to get base variant and networking type
  # "grafana-tap" -> { base = "grafana"; networking = "tap"; }
  # "grafana"     -> { base = "grafana"; networking = "user"; }
  parseVariant = name:
    if lib.hasSuffix "-tap" name then {
      base = lib.removeSuffix "-tap" name;
      networking = "tap";
    } else {
      base = name;
      networking = "user";
    };

  # Generate all phase scripts for a variant
  mkVariantScripts = variant: {
    check-process = lifecycleLib.mkCheckProcessScript { inherit variant; };
    check-serial = lifecycleLib.mkCheckSerialScript { inherit variant; };
    check-virtio = lifecycleLib.mkCheckVirtioScript { inherit variant; };
    verify-services = pcpChecks.mkVerifyServicesScript { inherit variant; };
    verify-metrics = pcpChecks.mkVerifyMetricsScript { inherit variant; };
    verify-http = pcpChecks.mkVerifyHttpScript { inherit variant; };
    verify-full = pcpChecks.mkFullVerificationScript { inherit variant; };
    shutdown = lifecycleLib.mkShutdownScript { inherit variant; };
    wait-exit = lifecycleLib.mkWaitExitScript { inherit variant; };
    force-kill = lifecycleLib.mkForceKillScript { inherit variant; };
    status = lifecycleLib.mkStatusScript { inherit variant; };
  };

  # Generate a full lifecycle test for a variant
  mkFullTest = variant:
    let
      # Parse variant name to get base variant and networking type
      parsed = parseVariant variant;
      baseVariant = parsed.base;
      isTap = parsed.networking == "tap";

      # Use base variant for configuration lookups
      variantConfig = constants.variants.${baseVariant};
      hostname = mainConstants.getHostname baseVariant;
      consolePorts = mainConstants.getConsolePorts baseVariant;
      portOffset = mainConstants.variantPortOffsets.${baseVariant};

      # TAP uses direct IP and standard SSH port; user-mode uses localhost with forwarding
      sshHost = if isTap then mainConstants.network.vmIp else "localhost";
      sshPort = if isTap then 22 else mainConstants.ports.sshForward + portOffset;

      # Get timeouts (use base variant)
      buildTimeout = mainConstants.getTimeout baseVariant "build";
      processTimeout = mainConstants.getTimeout baseVariant "processStart";
      serialTimeout = mainConstants.getTimeout baseVariant "serialReady";
      virtioTimeout = mainConstants.getTimeout baseVariant "virtioReady";
      serviceTimeout = mainConstants.getTimeout baseVariant "serviceReady";
      metricTimeout = mainConstants.getTimeout baseVariant "metricsReady";
      shutdownTimeout = mainConstants.getTimeout baseVariant "shutdown";
      exitTimeout = mainConstants.getTimeout baseVariant "waitExit";

      # Package name in flake (TAP variants have -tap suffix)
      packageName =
        if baseVariant == "base" then
          if isTap then "pcp-microvm-tap" else "pcp-microvm"
        else
          if isTap then "pcp-microvm-${baseVariant}-tap" else "pcp-microvm-${baseVariant}";

      # SSH options
      sshOpts = lib.concatStringsSep " " [
        "-o" "StrictHostKeyChecking=no"
        "-o" "UserKnownHostsFile=/dev/null"
        "-o" "ConnectTimeout=5"
        "-o" "LogLevel=ERROR"
        "-o" "PubkeyAuthentication=no"
      ];
    in
    pkgs.writeShellApplication {
      name = "pcp-lifecycle-full-test-${variant}";
      runtimeInputs = commonInputs ++ sshInputs ++ [ pkgs.curl pkgs.nix ];
      text = ''
        set +e  # Don't exit on first failure

        ${colorHelpers}
        ${timingHelpers}
        ${processHelpers}
        ${consoleHelpers}

        # SSH helpers
        ssh_cmd() {
          local host="$1"
          local port="$2"
          shift 2
          sshpass -p pcp ssh ${sshOpts} -p "$port" "root@$host" "$@" 2>/dev/null
        }

        check_service() {
          local host="$1"
          local port="$2"
          local service="$3"
          ssh_cmd "$host" "$port" "systemctl is-active $service" 2>/dev/null | grep -q "^active$"
        }

        # Wait for a service to become active (handles Type=oneshot services)
        # Some services like pmlogger use Type=oneshot and take time to start
        wait_for_service() {
          local host="$1"
          local port="$2"
          local service="$3"
          local timeout="$4"
          local elapsed=0
          while [[ $elapsed -lt $timeout ]]; do
            local status
            status=$(ssh_cmd "$host" "$port" "systemctl is-active $service" 2>/dev/null || echo "unknown")
            case "$status" in
              active)
                return 0
                ;;
              activating|inactive|unknown)
                # Service is starting, waiting to start, or we couldn't get status
                # Keep waiting
                sleep 1
                elapsed=$((elapsed + 1))
                ;;
              failed)
                # Service explicitly failed
                return 1
                ;;
              *)
                # Other states (deactivating, etc.) - keep waiting
                sleep 1
                elapsed=$((elapsed + 1))
                ;;
            esac
          done
          return 1
        }

        check_metric() {
          local host="$1"
          local port="$2"
          local metric="$3"
          ssh_cmd "$host" "$port" "pminfo -f $metric 2>/dev/null | grep -q value"
        }

        wait_for_ssh() {
          local host="$1"
          local port="$2"
          local timeout="$3"
          local elapsed=0
          while [[ $elapsed -lt $timeout ]]; do
            if sshpass -p pcp ssh ${sshOpts} -p "$port" "root@$host" true 2>/dev/null; then
              return 0
            fi
            sleep 1
            elapsed=$((elapsed + 1))
          done
          return 1
        }

        # Configuration
        VARIANT="${variant}"
        HOSTNAME="${hostname}"
        PACKAGE_NAME="${packageName}"
        SERIAL_PORT=${toString consolePorts.serial}
        VIRTIO_PORT=${toString consolePorts.virtio}
        SSH_HOST="${sshHost}"
        SSH_PORT=${toString sshPort}
        IS_TAP="${if isTap then "true" else "false"}"
        RESULT_LINK="result-lifecycle-$VARIANT"

        # Timing tracking
        declare -A PHASE_TIMES
        TOTAL_START=$(time_ms)

        # Results tracking
        TOTAL_PASSED=0
        TOTAL_FAILED=0

        record_result() {
          local phase="$1"
          local passed="$2"
          local time_ms="$3"
          PHASE_TIMES["$phase"]=$time_ms
          if [[ "$passed" == "true" ]]; then
            TOTAL_PASSED=$((TOTAL_PASSED + 1))
          else
            TOTAL_FAILED=$((TOTAL_FAILED + 1))
          fi
        }

        # Header
        bold "========================================"
        bold "  PCP MicroVM Full Lifecycle Test ($VARIANT)"
        bold "========================================"
        echo ""
        info "Description: ${variantConfig.description}"
        info "Hostname: $HOSTNAME"
        info "SSH: $SSH_HOST:$SSH_PORT"
        if [[ "$IS_TAP" == "true" ]]; then
          warn "TAP networking - requires host network setup (nix run .#pcp-network-setup)"
        fi
        echo ""

        # ─── Phase 0: Build VM ─────────────────────────────────────────────
        phase_header "0" "Build VM" "${toString buildTimeout}"
        start_time=$(time_ms)

        # Clean up any existing result link
        rm -f "$RESULT_LINK"

        info "  Building $PACKAGE_NAME..."
        if nix build ".#$PACKAGE_NAME" -o "$RESULT_LINK" 2>&1 | while read -r line; do
          echo "    $line"
        done; then
          elapsed=$(elapsed_ms "$start_time")
          result_pass "VM built" "$elapsed"
          record_result "build" "true" "$elapsed"
        else
          elapsed=$(elapsed_ms "$start_time")
          result_fail "Build failed" "$elapsed"
          record_result "build" "false" "$elapsed"
          exit 1
        fi

        # ─── Phase 1: Start VM ─────────────────────────────────────────────
        phase_header "1" "Start VM" "${toString processTimeout}"
        start_time=$(time_ms)

        # Kill any existing VM with this hostname
        if vm_is_running "$HOSTNAME"; then
          warn "  Killing existing VM..."
          kill_vm "$HOSTNAME"
          sleep 2
        fi

        info "  Starting $RESULT_LINK/bin/microvm-run..."
        "$RESULT_LINK/bin/microvm-run" &
        _bg_pid=$!  # Background PID (VM spawns its own QEMU process)

        if wait_for_process "$HOSTNAME" "${toString processTimeout}"; then
          elapsed=$(elapsed_ms "$start_time")
          qemu_pid=$(vm_pid "$HOSTNAME")
          result_pass "VM process running (PID: $qemu_pid, launcher: $_bg_pid)" "$elapsed"
          record_result "start" "true" "$elapsed"
        else
          elapsed=$(elapsed_ms "$start_time")
          result_fail "VM process not found" "$elapsed"
          record_result "start" "false" "$elapsed"
          rm -f "$RESULT_LINK"
          exit 1
        fi

        # ─── Phase 2: Check Serial Console ─────────────────────────────────
        phase_header "2" "Check Serial Console" "${toString serialTimeout}"
        start_time=$(time_ms)

        if wait_for_console "$SERIAL_PORT" "${toString serialTimeout}"; then
          elapsed=$(elapsed_ms "$start_time")
          result_pass "Serial console available (port $SERIAL_PORT)" "$elapsed"
          record_result "serial" "true" "$elapsed"
        else
          elapsed=$(elapsed_ms "$start_time")
          result_fail "Serial console not available" "$elapsed"
          record_result "serial" "false" "$elapsed"
        fi

        # ─── Phase 2b: Check Virtio Console ────────────────────────────────
        phase_header "2b" "Check Virtio Console" "${toString virtioTimeout}"
        start_time=$(time_ms)

        if wait_for_console "$VIRTIO_PORT" "${toString virtioTimeout}"; then
          elapsed=$(elapsed_ms "$start_time")
          result_pass "Virtio console available (port $VIRTIO_PORT)" "$elapsed"
          record_result "virtio" "true" "$elapsed"
        else
          elapsed=$(elapsed_ms "$start_time")
          result_fail "Virtio console not available" "$elapsed"
          record_result "virtio" "false" "$elapsed"
        fi

        # ─── Phase 3: Verify PCP Services ──────────────────────────────────
        phase_header "3" "Verify PCP Services" "${toString serviceTimeout}"
        start_time=$(time_ms)

        info "  Waiting for SSH..."
        if ! wait_for_ssh "$SSH_HOST" "$SSH_PORT" "${toString serviceTimeout}"; then
          elapsed=$(elapsed_ms "$start_time")
          result_fail "SSH not available" "$elapsed"
          record_result "services" "false" "$elapsed"
        else
          ssh_elapsed=$(elapsed_ms "$start_time")
          info "  SSH connected (''${ssh_elapsed}ms)"

          service_passed=0
          service_failed=0

          # Services that use Type=oneshot and need extra wait time
          SLOW_SERVICES="pmlogger pmie"

          ${lib.concatMapStringsSep "\n" (service: ''
            svc_start=$(time_ms)
            # Check if this is a slow-starting service
            if [[ " $SLOW_SERVICES " == *" ${service} "* ]]; then
              # Wait up to 60s for Type=oneshot services
              if wait_for_service "$SSH_HOST" "$SSH_PORT" "${service}" 60; then
                result_pass "${service} active" "$(elapsed_ms "$svc_start")"
                service_passed=$((service_passed + 1))
              else
                result_fail "${service} not active" "$(elapsed_ms "$svc_start")"
                service_failed=$((service_failed + 1))
              fi
            else
              # Quick check for normal services
              if check_service "$SSH_HOST" "$SSH_PORT" "${service}"; then
                result_pass "${service} active" "$(elapsed_ms "$svc_start")"
                service_passed=$((service_passed + 1))
              else
                result_fail "${service} not active" "$(elapsed_ms "$svc_start")"
                service_failed=$((service_failed + 1))
              fi
            fi
          '') variantConfig.services}

          elapsed=$(elapsed_ms "$start_time")
          if [[ $service_failed -eq 0 ]]; then
            record_result "services" "true" "$elapsed"
          else
            record_result "services" "false" "$elapsed"
          fi
        fi

        # ─── Phase 4: Verify PCP Metrics ───────────────────────────────────
        phase_header "4" "Verify PCP Metrics" "${toString metricTimeout}"
        start_time=$(time_ms)

        metric_passed=0
        metric_failed=0

        ${lib.concatMapStringsSep "\n" (metric: ''
          met_start=$(time_ms)
          if check_metric "$SSH_HOST" "$SSH_PORT" "${metric}"; then
            result_pass "${metric}" "$(elapsed_ms "$met_start")"
            metric_passed=$((metric_passed + 1))
          else
            result_fail "${metric} not available" "$(elapsed_ms "$met_start")"
            metric_failed=$((metric_failed + 1))
          fi
        '') variantConfig.metrics}

        elapsed=$(elapsed_ms "$start_time")
        if [[ $metric_failed -eq 0 ]]; then
          record_result "metrics" "true" "$elapsed"
        else
          record_result "metrics" "false" "$elapsed"
        fi

        # ─── Phase 5: Shutdown ─────────────────────────────────────────────
        phase_header "5" "Shutdown" "${toString shutdownTimeout}"
        start_time=$(time_ms)

        info "  Sending shutdown command..."
        if ssh_cmd "$SSH_HOST" "$SSH_PORT" "poweroff" 2>/dev/null; then
          elapsed=$(elapsed_ms "$start_time")
          result_pass "Shutdown command sent" "$elapsed"
          record_result "shutdown" "true" "$elapsed"
        else
          elapsed=$(elapsed_ms "$start_time")
          warn "  SSH shutdown failed, killing process..."
          kill_vm "$HOSTNAME"
          result_pass "VM process killed" "$elapsed"
          record_result "shutdown" "true" "$elapsed"
        fi

        # ─── Phase 6: Wait for Exit ────────────────────────────────────────
        phase_header "6" "Wait for Exit" "${toString exitTimeout}"
        start_time=$(time_ms)

        if wait_for_exit "$HOSTNAME" "${toString exitTimeout}"; then
          elapsed=$(elapsed_ms "$start_time")
          result_pass "VM exited cleanly" "$elapsed"
          record_result "exit" "true" "$elapsed"
        else
          elapsed=$(elapsed_ms "$start_time")
          result_fail "VM did not exit, forcing kill" "$elapsed"
          kill_vm "$HOSTNAME"
          record_result "exit" "false" "$elapsed"
        fi

        # ─── Cleanup ───────────────────────────────────────────────────────
        rm -f "$RESULT_LINK"

        # ─── Summary ───────────────────────────────────────────────────────
        TOTAL_ELAPSED=$(elapsed_ms "$TOTAL_START")

        echo ""
        bold "  Timing Summary"
        echo "  $(printf '─%.0s' {1..37})"
        printf "  %-25s %10s\n" "Phase" "Time (ms)"
        echo "  $(printf '─%.0s' {1..37})"
        for phase in build start serial virtio services metrics shutdown exit; do
          if [[ -n "''${PHASE_TIMES[$phase]:-}" ]]; then
            printf "  %-25s %10s\n" "$phase" "''${PHASE_TIMES[$phase]}"
          fi
        done
        echo "  $(printf '─%.0s' {1..37})"
        printf "  %-25s %10s\n" "TOTAL" "$TOTAL_ELAPSED"
        echo "  $(printf '─%.0s' {1..37})"

        echo ""
        bold "========================================"
        if [[ $TOTAL_FAILED -eq 0 ]]; then
          success "  Result: ALL PHASES PASSED"
          success "  Total time: $(format_ms "$TOTAL_ELAPSED")"
        else
          error "  Result: $TOTAL_FAILED PHASES FAILED"
        fi
        bold "========================================"

        [[ $TOTAL_FAILED -eq 0 ]]
      '';
    };

  # Generate test-all script
  mkTestAll = pkgs.writeShellApplication {
    name = "pcp-lifecycle-test-all";
    runtimeInputs = commonInputs ++ sshInputs ++ [ pkgs.curl pkgs.nix ];
    text = ''
      set +e

      ${colorHelpers}
      ${timingHelpers}

      bold "========================================"
      bold "  PCP MicroVM Lifecycle Test Suite"
      bold "========================================"
      echo ""

      VARIANTS="${lib.concatStringsSep " " allVariantNames}"
      SKIP_VARIANTS=""
      ONLY_VARIANT=""

      # Parse arguments
      while [[ $# -gt 0 ]]; do
        case "$1" in
          --skip=*)
            SKIP_VARIANTS="''${1#--skip=}"
            shift
            ;;
          --only=*)
            ONLY_VARIANT="''${1#--only=}"
            shift
            ;;
          --help|-h)
            echo "Usage: pcp-lifecycle-test-all [OPTIONS]"
            echo ""
            echo "Options:"
            echo "  --skip=VARIANT   Skip specified variant (comma-separated)"
            echo "  --only=VARIANT   Test only specified variant"
            echo "  --help, -h       Show this help"
            echo ""
            echo "User-mode variants: ${lib.concatStringsSep " " variantNames}"
            echo "TAP variants: ${lib.concatStringsSep " " tapVariantNames}"
            echo ""
            echo "TAP variants require host network setup first:"
            echo "  nix run .#pcp-network-setup"
            exit 0
            ;;
          *)
            echo "Unknown option: $1"
            exit 1
            ;;
        esac
      done

      # Results tracking
      declare -A RESULTS
      declare -A DURATIONS
      TOTAL_PASSED=0
      TOTAL_FAILED=0
      TOTAL_SKIPPED=0

      TOTAL_START=$(time_ms)

      # Check if TAP network is available (for TAP variants)
      TAP_AVAILABLE="false"
      if ip link show ${mainConstants.network.bridge} >/dev/null 2>&1; then
        TAP_AVAILABLE="true"
        info "TAP network available (bridge: ${mainConstants.network.bridge})"
      else
        warn "TAP network not available - TAP variants will be skipped"
        warn "Run 'nix run .#pcp-network-setup' to enable TAP testing"
      fi
      echo ""

      for variant in $VARIANTS; do
        # Check if should skip
        if [[ -n "$ONLY_VARIANT" ]] && [[ "$variant" != "$ONLY_VARIANT" ]]; then
          RESULTS[$variant]="SKIPPED"
          DURATIONS[$variant]=0
          TOTAL_SKIPPED=$((TOTAL_SKIPPED + 1))
          continue
        fi

        if [[ "$SKIP_VARIANTS" == *"$variant"* ]]; then
          RESULTS[$variant]="SKIPPED"
          DURATIONS[$variant]=0
          TOTAL_SKIPPED=$((TOTAL_SKIPPED + 1))
          continue
        fi

        # Skip TAP variants if TAP network is not available
        if [[ "$variant" == *"-tap" ]] && [[ "$TAP_AVAILABLE" != "true" ]]; then
          RESULTS[$variant]="SKIPPED (no TAP)"
          DURATIONS[$variant]=0
          TOTAL_SKIPPED=$((TOTAL_SKIPPED + 1))
          continue
        fi

        echo ""
        bold "════════════════════════════════════════"
        bold "  Testing: $variant"
        bold "════════════════════════════════════════"

        variant_start=$(time_ms)

        # Run the full test for this variant
        test_script="pcp-lifecycle-full-test-$variant"
        if command -v "$test_script" >/dev/null 2>&1; then
          if "$test_script"; then
            RESULTS[$variant]="PASSED"
            TOTAL_PASSED=$((TOTAL_PASSED + 1))
          else
            RESULTS[$variant]="FAILED"
            TOTAL_FAILED=$((TOTAL_FAILED + 1))
          fi
        else
          # Try running via nix run
          if nix run ".#$test_script" 2>/dev/null; then
            RESULTS[$variant]="PASSED"
            TOTAL_PASSED=$((TOTAL_PASSED + 1))
          else
            RESULTS[$variant]="FAILED"
            TOTAL_FAILED=$((TOTAL_FAILED + 1))
          fi
        fi

        DURATIONS[$variant]=$(elapsed_ms "$variant_start")
      done

      TOTAL_ELAPSED=$(elapsed_ms "$TOTAL_START")

      # Summary
      echo ""
      bold "========================================"
      bold "  Test Suite Summary"
      bold "========================================"
      echo ""

      printf "%-12s %-15s %12s\n" "Variant" "Result" "Duration"
      printf "%-12s %-15s %12s\n" "───────" "──────" "────────"

      for variant in $VARIANTS; do
        result="''${RESULTS[$variant]:-UNKNOWN}"
        duration="''${DURATIONS[$variant]:-0}"

        if [[ "$result" == "PASSED" ]]; then
          printf "%-12s \033[32m%-15s\033[0m %12s\n" "$variant" "$result" "$(format_ms "$duration")"
        elif [[ "$result" == "FAILED" ]]; then
          printf "%-12s \033[31m%-15s\033[0m %12s\n" "$variant" "$result" "$(format_ms "$duration")"
        else
          printf "%-12s \033[33m%-15s\033[0m %12s\n" "$variant" "$result" "-"
        fi
      done

      echo ""
      echo "────────────────────────────────────────"
      echo "Total: $TOTAL_PASSED passed, $TOTAL_FAILED failed, $TOTAL_SKIPPED skipped"
      echo "Total time: $(format_ms "$TOTAL_ELAPSED")"
      echo "────────────────────────────────────────"

      [[ $TOTAL_FAILED -eq 0 ]]
    '';
  };

  # Generate all scripts for all variants (user-mode only, TAP shares scripts)
  lifecycleByVariant = lib.genAttrs variantNames (variant: mkVariantScripts variant);

  # Generate full tests for all variants (including TAP)
  testsByVariant = lib.genAttrs allVariantNames (variant: mkFullTest variant);

in
{
  # Individual phase scripts by variant
  # Usage: lifecycle.scripts.base.check-process
  scripts = lifecycleByVariant;

  # Full lifecycle tests by variant
  # Usage: lifecycle.tests.base
  tests = testsByVariant // {
    all = mkTestAll;
  };

  # Flattened package set for flake.nix packages output
  # Usage: packages.pcp-lifecycle-full-test-base
  packages =
    let
      # Flatten scripts: pcp-lifecycle-<phase>-<variant>
      flattenedScripts = lib.foldl' (acc: variant:
        acc // (lib.mapAttrs' (phase: script:
          lib.nameValuePair "pcp-lifecycle-${phase}-${variant}" script
        ) lifecycleByVariant.${variant})
      ) {} variantNames;

      # Full tests: pcp-lifecycle-full-test-<variant>
      fullTests = lib.mapAttrs' (variant: test:
        lib.nameValuePair "pcp-lifecycle-full-test-${variant}" test
      ) testsByVariant;
    in
    flattenedScripts // fullTests // {
      pcp-lifecycle-test-all = mkTestAll;
    };

  # Apps output for flake.nix
  apps = lib.mapAttrs (name: pkg: {
    type = "app";
    program = "${pkg}/bin/${name}";
  }) (
    let
      # Full tests for all variants (including TAP)
      fullTestApps = lib.foldl' (acc: variant:
        acc // {
          "pcp-lifecycle-full-test-${variant}" = testsByVariant.${variant};
        }
      ) {} allVariantNames;

      # Status and force-kill for user-mode variants only (TAP shares base scripts)
      utilityApps = lib.foldl' (acc: variant:
        acc // {
          "pcp-lifecycle-status-${variant}" = lifecycleByVariant.${variant}.status;
          "pcp-lifecycle-force-kill-${variant}" = lifecycleByVariant.${variant}.force-kill;
        }
      ) {} variantNames;
    in
    fullTestApps // utilityApps // {
      pcp-lifecycle-test-all = mkTestAll;
    }
  );
}
