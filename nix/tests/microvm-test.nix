# nix/tests/microvm-test.nix
#
# Test script builder for MicroVM variants.
# Uses constants.nix for all configuration.
#
# Usage:
#   nix run .#pcp-test-base-user     # Test base VM with user networking
#   nix run .#pcp-test-base-tap      # Test base VM with TAP networking
#   nix run .#pcp-test-eval-user     # Test eval VM with user networking
#   nix run .#pcp-test-eval-tap      # Test eval VM with TAP networking
#
{ pkgs, lib, variant, host, sshPort }:
let
  testLib = import ../test-lib.nix { inherit pkgs lib; };
  constants = testLib.constants;

  isEval = lib.hasPrefix "eval" variant;
  isTap = lib.hasSuffix "tap" variant;
in
pkgs.writeShellApplication {
  name = "pcp-test-${variant}";
  runtimeInputs = with pkgs; [ openssh sshpass curl bc coreutils ];
  text = ''
    echo "========================================================"
    echo "  PCP MicroVM Test: ${variant}"
    echo "  Host: ${host}  SSH Port: ${toString sshPort}"
    echo "========================================================"
    echo ""

    # Create output directory
    output_dir="test-results/${variant}"
    mkdir -p "$output_dir"

    passed=0
    failed=0

    check() {
      if "$@"; then
        passed=$((passed + 1))
      else
        failed=$((failed + 1))
      fi
    }

    ${testLib.waitForSsh}
    ${testLib.waitForService}
    ${testLib.runCheck}
    ${testLib.checkService}
    ${testLib.checkPort}
    ${testLib.checkMetric}
    ${testLib.checkJournal}
    ${testLib.checkTui}
    ${testLib.checkSecurity}
    ${testLib.checkPmns}
    ${lib.optionalString isEval testLib.checkMetricParity}
    ${lib.optionalString isEval testLib.checkPmieTest}

    # Phase 1: SSH connectivity
    echo ""
    echo "Phase 1: SSH connectivity"
    wait_for_ssh "${host}" "${toString sshPort}"

    # Phase 2: Service status
    echo ""
    echo "Phase 2: Service status"
    wait_for_service "${host}" "${toString sshPort}" "pmcd" "${toString constants.ports.pmcd}"
    check check_service "${host}" "${toString sshPort}" "pmcd"
    check check_service "${host}" "${toString sshPort}" "pmproxy"
    check check_port "${host}" "${toString sshPort}" "${toString constants.ports.pmcd}"
    check check_port "${host}" "${toString sshPort}" "${toString constants.ports.pmproxy}"
    ${lib.optionalString isEval ''
    check check_service "${host}" "${toString sshPort}" "prometheus-node-exporter"
    check check_port "${host}" "${toString sshPort}" "${toString constants.ports.nodeExporter}"
    ''}

    # Phase 3: PCP metrics
    echo ""
    echo "Phase 3: PCP metrics"
    check check_pmns "${host}" "${toString sshPort}"
    check check_metric "${host}" "${toString sshPort}" "kernel.all.load"
    check check_metric "${host}" "${toString sshPort}" "kernel.all.cpu.user"
    check check_metric "${host}" "${toString sshPort}" "mem.physmem"

    # Phase 4: HTTP endpoints
    echo ""
    echo "Phase 4: HTTP endpoints"
    check run_check "pmproxy REST API" "${host}" "${toString sshPort}" \
      "curl -sf http://localhost:${toString constants.ports.pmproxy}/pmapi/1/metrics?target=kernel.all.load | head -1"
    ${lib.optionalString isEval ''
    check run_check "node_exporter metrics" "${host}" "${toString sshPort}" \
      "curl -sf http://localhost:${toString constants.ports.nodeExporter}/metrics | grep -q node_cpu_seconds_total"
    ''}

    # Phase 5: Journal health
    echo ""
    echo "Phase 5: Journal health"
    check check_journal "${host}" "${toString sshPort}" "pmcd"
    check check_journal "${host}" "${toString sshPort}" "pmproxy"

    # Phase 6: PCP TUI smoke test
    echo ""
    echo "Phase 6: PCP TUI smoke test"
    check check_tui "${host}" "${toString sshPort}" "pcp-dstat outputs" \
      "pcp dstat --nocolor 1 2"

    ${lib.optionalString isEval ''
    # Phase 7: Metric parity (eval only)
    echo ""
    echo "Phase 7: Metric parity"
    check check_metric_parity "${host}" "${toString sshPort}" \
      "kernel.all.load" "node_load1"

    # Phase 8: pmie testing (eval only)
    echo ""
    echo "Phase 8: pmie testing"
    check check_pmie_test "${host}" "${toString sshPort}" 45
    ''}

    # Summary
    echo ""
    echo "========================================================"
    echo "  Results: $passed passed, $failed failed"
    echo "========================================================"

    # Save results
    {
      echo "variant=${variant}"
      echo "passed=$passed"
      echo "failed=$failed"
      echo "timestamp=$(date -Iseconds)"
    } > "$output_dir/results.txt"

    if [[ $failed -gt 0 ]]; then
      exit 1
    fi
  '';
}
