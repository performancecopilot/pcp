# nix/test-lib.nix
#
# Shared test functions for MicroVM validation.
# All timeouts, thresholds, and ports come from constants.nix.
#
# This library provides shell script fragments that can be composed
# into test scripts. Each function is a string containing bash code.
#
{ pkgs, lib }:
let
  constants = import ./constants.nix;

  sshOpts = [
    "-o" "StrictHostKeyChecking=no"
    "-o" "UserKnownHostsFile=/dev/null"
    "-o" "ConnectTimeout=${toString constants.test.sshTimeoutSeconds}"
    "-o" "LogLevel=ERROR"
    "-o" "PubkeyAuthentication=no"
  ];
  sshOptsStr = lib.concatStringsSep " " sshOpts;

  # SSH command with sshpass for debug VMs (password: pcp)
  sshPassCmd = "sshpass -p pcp ssh";
in
{
  inherit sshOpts sshOptsStr constants;

  # SSH connectivity
  # Polling loop instead of fixed sleep. Respects constants.test.sshMaxAttempts.
  waitForSsh = ''
    wait_for_ssh() {
      local host="$1" port="$2"
      local max="${toString constants.test.sshMaxAttempts}"
      local delay="${toString constants.test.sshRetryDelaySeconds}"
      local attempt=0

      echo "Waiting for SSH on $host:$port (max $max attempts)..."
      while ! sshpass -p pcp ssh ${sshOptsStr} -p "$port" "root@$host" true 2>/dev/null; do
        attempt=$((attempt + 1))
        if [[ $attempt -ge $max ]]; then
          echo "FAIL: SSH not available after $max attempts"
          return 1
        fi
        echo "  attempt $attempt/$max..."
        sleep "$delay"
      done
      echo "SSH connected to $host:$port"
    }
  '';

  # Service readiness
  # Wait for a service to be active AND its port to be listening.
  waitForService = ''
    wait_for_service() {
      local host="$1" port="$2" service="$3" target_port="$4"
      local max=30
      local attempt=0

      echo -n "  Waiting for $service on port $target_port... "
      while true; do
        if sshpass -p pcp ssh ${sshOptsStr} -p "$port" "root@$host" \
            "systemctl is-active $service >/dev/null 2>&1 && ss -tlnp | grep -q :$target_port" 2>/dev/null; then
          echo "ready"
          return 0
        fi

        attempt=$((attempt + 1))
        if [[ $attempt -ge $max ]]; then
          echo "timeout"
          return 1
        fi
        sleep 2
      done
    }
  '';

  # Basic check runner
  runCheck = ''
    run_check() {
      local desc="$1" host="$2" port="$3"
      shift 3
      local cmd="$*"
      echo -n "  CHECK: $desc ... "
      local output
      if output=$(sshpass -p pcp ssh ${sshOptsStr} -p "$port" "root@$host" "$cmd" 2>&1); then
        echo "OK"
        if [[ -n "$output" ]]; then
          echo "$output" | head -5 | while IFS= read -r line; do echo "    $line"; done
        fi
        return 0
      else
        echo "FAIL"
        echo "    command: $cmd"
        echo "    output:  $output"
        return 1
      fi
    }
  '';

  # Service check
  checkService = ''
    check_service() {
      run_check "service $3 is active" "$1" "$2" "systemctl is-active $3"
    }
  '';

  # Port check
  checkPort = ''
    check_port() {
      run_check "port $3 is listening" "$1" "$2" "ss -tlnp | grep -q :$3"
    }
  '';

  # Metric check
  checkMetric = ''
    check_metric() {
      run_check "metric $3" "$1" "$2" "pminfo -f $3 2>/dev/null | head -3"
    }
  '';

  # Journal error checking
  checkJournal = ''
    check_journal() {
      local host="$1" port="$2" service="$3"
      echo -n "  CHECK: no errors in $service journal ... "
      local errors
      errors=$(sshpass -p pcp ssh ${sshOptsStr} -p "$port" "root@$host" \
        "journalctl -u $service --no-pager -p err -q --no-hostname 2>/dev/null" 2>&1)
      if [[ -z "$errors" ]]; then
        echo "OK (clean)"
        return 0
      else
        local count
        count=$(echo "$errors" | wc -l)
        echo "FAIL ($count error line(s))"
        while IFS= read -r line; do
          echo "    $line"
        done <<< "$errors"
        return 1
      fi
    }
  '';

  # TUI smoke test
  checkTui = ''
    check_tui() {
      local host="$1" port="$2" desc="$3"
      shift 3
      local cmd="$*"
      echo -n "  CHECK: $desc ... "
      local output exit_code
      output=$(sshpass -p pcp ssh ${sshOptsStr} -p "$port" "root@$host" \
        "timeout 10 $cmd 2>&1" 2>&1) && exit_code=$? || exit_code=$?
      # 124 = timeout (acceptable), 0 = clean, others = failure
      if [[ $exit_code -eq 0 || $exit_code -eq 124 ]]; then
        echo "OK (exit $exit_code)"
        return 0
      else
        echo "FAIL (exit $exit_code)"
        echo "    command: $cmd"
        echo "$output" | tail -5 | while IFS= read -r line; do echo "    $line"; done
        return 1
      fi
    }
  '';

  # Security analysis using systemd-analyze security
  checkSecurity = ''
    check_security() {
      local host="$1" port="$2" service="$3" network_facing="''${4:-false}"
      local max_score="${toString constants.security.networkServiceMaxScore}"
      local warn_score="${toString constants.security.internalServiceWarnScore}"

      echo -n "  CHECK: systemd security $service ... "
      local output score_line score level

      output=$(sshpass -p pcp ssh ${sshOptsStr} -p "$port" "root@$host" \
        "systemd-analyze security $service 2>&1" 2>&1)

      score_line=$(echo "$output" | grep -E "^→ Overall exposure level" | tail -1)

      if [[ -z "$score_line" ]]; then
        echo "FAIL (could not parse security output)"
        return 1
      fi

      score=$(echo "$score_line" | grep -oE '[0-9]+\.[0-9]+' | head -1)
      level=$(echo "$score_line" | awk '{print $NF}')

      if [[ "$network_facing" == "true" ]]; then
        if (( $(echo "$score > $max_score" | bc -l) )); then
          echo "FAIL (score $score $level, must be <= $max_score)"
          return 1
        else
          echo "OK (score $score $level)"
          return 0
        fi
      else
        if (( $(echo "$score > $warn_score" | bc -l) )); then
          echo "WARN (score $score $level, consider hardening)"
          return 0
        else
          echo "OK (score $score $level)"
          return 0
        fi
      fi
    }
  '';

  # PMNS verification
  checkPmns = ''
    check_pmns() {
      local host="$1" port="$2"
      local min_metrics="${toString constants.test.minExpectedMetrics}"

      echo -n "  CHECK: PMNS root loaded ... "
      if sshpass -p pcp ssh ${sshOptsStr} -p "$port" "root@$host" \
          "pminfo >/dev/null 2>&1"; then
        echo "OK"
      else
        echo "FAIL"
        return 1
      fi

      echo -n "  CHECK: metric count >= $min_metrics ... "
      local count
      count=$(sshpass -p pcp ssh ${sshOptsStr} -p "$port" "root@$host" "pminfo 2>/dev/null | wc -l")
      if [[ "$count" -ge "$min_metrics" ]]; then
        echo "OK ($count metrics)"
      else
        echo "WARN (only $count metrics)"
      fi

      return 0
    }
  '';

  # pmie testing check
  # Verifies pmie-test and stress-ng-test services are running and generating alerts
  checkPmieTest = ''
    check_pmie_test() {
      local host="$1" port="$2"
      local wait_time="$3"  # Seconds to wait for alerts

      echo -n "  CHECK: service pmie-test is active ... "
      if sshpass -p pcp ssh ${sshOptsStr} -p "$port" "root@$host" \
          "systemctl is-active pmie-test" >/dev/null 2>&1; then
        echo "OK"
      else
        echo "FAIL"
        return 1
      fi

      echo -n "  CHECK: service stress-ng-test is active ... "
      if sshpass -p pcp ssh ${sshOptsStr} -p "$port" "root@$host" \
          "systemctl is-active stress-ng-test" >/dev/null 2>&1; then
        echo "OK"
      else
        echo "FAIL"
        return 1
      fi

      echo "  Waiting ''${wait_time}s for pmie to detect stress cycles..."
      sleep "$wait_time"

      echo -n "  CHECK: pmie alerts.log has content ... "
      if sshpass -p pcp ssh ${sshOptsStr} -p "$port" "root@$host" \
          "test -s /var/log/pcp/pmie/alerts.log" 2>/dev/null; then
        echo "OK"
        sshpass -p pcp ssh ${sshOptsStr} -p "$port" "root@$host" \
          "tail -3 /var/log/pcp/pmie/alerts.log" 2>/dev/null | while IFS= read -r line; do
            echo "    $line"
          done
      else
        echo "FAIL (empty or missing)"
        return 1
      fi

      echo -n "  CHECK: pmie heartbeat file exists ... "
      if sshpass -p pcp ssh ${sshOptsStr} -p "$port" "root@$host" \
          "test -f /var/log/pcp/pmie/heartbeat" 2>/dev/null; then
        echo "OK"
      else
        echo "FAIL"
        return 1
      fi

      echo -n "  CHECK: pmie detected CPU elevation ... "
      if sshpass -p pcp ssh ${sshOptsStr} -p "$port" "root@$host" \
          "grep -q 'ALERT.*CPU elevated' /var/log/pcp/pmie/alerts.log" 2>/dev/null; then
        echo "OK"
      else
        echo "WARN (no CPU alerts yet)"
      fi

      return 0
    }
  '';

  # Metric parity check (PCP vs node_exporter)
  checkMetricParity = ''
    check_metric_parity() {
      local host="$1" port="$2" pcp_metric="$3" prom_query="$4"
      local tolerance="${toString constants.test.metricParityTolerancePct}"

      echo -n "  CHECK: parity $pcp_metric vs $prom_query ... "

      local pcp_val prom_val
      pcp_val=$(sshpass -p pcp ssh ${sshOptsStr} -p "$port" "root@$host" \
        "pmval -s 1 -f 4 $pcp_metric 2>/dev/null | tail -1 | awk '{print \$NF}'" 2>&1)
      prom_val=$(sshpass -p pcp ssh ${sshOptsStr} -p "$port" "root@$host" \
        "curl -sf localhost:${toString constants.ports.nodeExporter}/metrics | grep -E '^$prom_query ' | head -1 | awk '{print \$2}'" 2>&1)

      if ! [[ "$pcp_val" =~ ^[0-9.]+$ ]] || ! [[ "$prom_val" =~ ^[0-9.]+$ ]]; then
        echo "SKIP (non-numeric values)"
        return 0
      fi

      local pct
      if (( $(echo "$prom_val != 0" | bc -l) )); then
        pct=$(echo "scale=2; 100 * ($pcp_val - $prom_val) / $prom_val" | bc -l)
        pct=''${pct#-}
      else
        pct="0"
      fi

      if (( $(echo "$pct <= $tolerance" | bc -l) )); then
        echo "OK (diff $pct%)"
        return 0
      else
        echo "WARN (diff $pct% > $tolerance%)"
        return 0
      fi
    }
  '';

  # ─── Grafana Tests ────────────────────────────────────────────────────────
  # Tests for Grafana + Prometheus comparison stack

  # Check Grafana service and HTTP endpoint
  checkGrafana = ''
    check_grafana() {
      local host="$1" port="$2"

      echo -n "  CHECK: service grafana is active ... "
      if sshpass -p pcp ssh ${sshOptsStr} -p "$port" "root@$host" \
          "systemctl is-active grafana" >/dev/null 2>&1; then
        echo "OK"
      else
        echo "FAIL"
        return 1
      fi

      echo -n "  CHECK: Grafana HTTP responds on port ${toString constants.ports.grafana} ... "
      if sshpass -p pcp ssh ${sshOptsStr} -p "$port" "root@$host" \
          "curl -sf http://localhost:${toString constants.ports.grafana}/api/health" >/dev/null 2>&1; then
        echo "OK"
      else
        echo "FAIL"
        return 1
      fi

      echo -n "  CHECK: Grafana login works (admin/pcp) ... "
      if sshpass -p pcp ssh ${sshOptsStr} -p "$port" "root@$host" \
          "curl -sf -u admin:pcp http://localhost:${toString constants.ports.grafana}/api/org" >/dev/null 2>&1; then
        echo "OK"
      else
        echo "FAIL"
        return 1
      fi

      return 0
    }
  '';

  # Check Prometheus service and HTTP endpoint
  checkPrometheus = ''
    check_prometheus() {
      local host="$1" port="$2"

      echo -n "  CHECK: service prometheus is active ... "
      if sshpass -p pcp ssh ${sshOptsStr} -p "$port" "root@$host" \
          "systemctl is-active prometheus" >/dev/null 2>&1; then
        echo "OK"
      else
        echo "FAIL"
        return 1
      fi

      echo -n "  CHECK: Prometheus HTTP responds on port ${toString constants.ports.prometheus} ... "
      if sshpass -p pcp ssh ${sshOptsStr} -p "$port" "root@$host" \
          "curl -sf http://localhost:${toString constants.ports.prometheus}/-/ready" >/dev/null 2>&1; then
        echo "OK"
      else
        echo "FAIL"
        return 1
      fi

      echo -n "  CHECK: Prometheus query API works ... "
      if sshpass -p pcp ssh ${sshOptsStr} -p "$port" "root@$host" \
          "curl -sf 'http://localhost:${toString constants.ports.prometheus}/api/v1/query?query=up' | grep -q success" 2>/dev/null; then
        echo "OK"
      else
        echo "FAIL"
        return 1
      fi

      return 0
    }
  '';

  # Check Grafana datasources are provisioned
  checkGrafanaDatasources = ''
    check_grafana_datasources() {
      local host="$1" port="$2"

      echo -n "  CHECK: PCP Vector datasource provisioned ... "
      if sshpass -p pcp ssh ${sshOptsStr} -p "$port" "root@$host" \
          "curl -sf -u admin:pcp http://localhost:${toString constants.ports.grafana}/api/datasources | grep -q 'PCP Vector'" 2>/dev/null; then
        echo "OK"
      else
        echo "FAIL"
        return 1
      fi

      echo -n "  CHECK: Prometheus datasource provisioned ... "
      if sshpass -p pcp ssh ${sshOptsStr} -p "$port" "root@$host" \
          "curl -sf -u admin:pcp http://localhost:${toString constants.ports.grafana}/api/datasources | grep -q 'Prometheus'" 2>/dev/null; then
        echo "OK"
      else
        echo "FAIL"
        return 1
      fi

      return 0
    }
  '';

  # Check Grafana dashboards are loaded
  checkGrafanaDashboards = ''
    check_grafana_dashboards() {
      local host="$1" port="$2"

      echo -n "  CHECK: PCP dashboards folder exists ... "
      if sshpass -p pcp ssh ${sshOptsStr} -p "$port" "root@$host" \
          "curl -sf -u admin:pcp http://localhost:${toString constants.ports.grafana}/api/search?folderIds=0 | grep -q 'PCP'" 2>/dev/null; then
        echo "OK"
      else
        echo "FAIL"
        return 1
      fi

      echo -n "  CHECK: Host Overview dashboard loaded ... "
      if sshpass -p pcp ssh ${sshOptsStr} -p "$port" "root@$host" \
          "curl -sf -u admin:pcp http://localhost:${toString constants.ports.grafana}/api/search | grep -q 'Host Overview'" 2>/dev/null; then
        echo "OK"
      else
        echo "FAIL"
        return 1
      fi

      echo -n "  CHECK: Node Exporter dashboard loaded ... "
      if sshpass -p pcp ssh ${sshOptsStr} -p "$port" "root@$host" \
          "curl -sf -u admin:pcp http://localhost:${toString constants.ports.grafana}/api/search | grep -qi 'node'" 2>/dev/null; then
        echo "OK"
      else
        echo "FAIL"
        return 1
      fi

      return 0
    }
  '';

  # Full Grafana test suite
  checkGrafanaFull = ''
    check_grafana_full() {
      local host="$1" port="$2"

      echo "=== Grafana Tests ==="
      check_grafana "$host" "$port" || return 1
      check_prometheus "$host" "$port" || return 1
      check_grafana_datasources "$host" "$port" || return 1
      check_grafana_dashboards "$host" "$port" || return 1

      # Security checks
      check_security "$host" "$port" "grafana.service" true || return 1
      check_security "$host" "$port" "prometheus.service" true || return 1

      return 0
    }
  '';
}
