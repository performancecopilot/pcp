# nix/container-test/lib.nix
#
# Shell helper functions for PCP container lifecycle testing.
# Provides reusable shell code for color output, timing, and container operations.
#
{ pkgs, lib }:
let
  constants = import ./constants.nix { };
  mainConstants = import ../constants.nix;
in
rec {
  # Common runtime inputs for container test scripts
  commonInputs = with pkgs; [
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

  # Container runtime inputs (docker or podman)
  containerInputs = with pkgs; [
    docker
  ];

  # ─── Color Helpers ──────────────────────────────────────────────────────
  # ANSI color codes for terminal output (reused from lifecycle/lib.nix)
  colorHelpers = ''
    # ANSI color codes
    _reset='\033[0m'
    _bold='\033[1m'
    _red='\033[31m'
    _green='\033[32m'
    _yellow='\033[33m'
    _blue='\033[34m'
    _cyan='\033[36m'

    # Color output functions
    info() { echo -e "''${_cyan}$*''${_reset}"; }
    success() { echo -e "''${_green}$*''${_reset}"; }
    warn() { echo -e "''${_yellow}$*''${_reset}"; }
    error() { echo -e "''${_red}$*''${_reset}"; }
    bold() { echo -e "''${_bold}$*''${_reset}"; }

    # Phase header
    phase_header() {
      local phase="$1"
      local name="$2"
      local timeout="$3"
      echo ""
      echo -e "''${_bold}--- Phase $phase: $name (timeout: ''${timeout}s) ---''${_reset}"
    }

    # Pass/fail result with timing
    result_pass() {
      local msg="$1"
      local time_ms="''${2:-}"
      if [[ -n "$time_ms" ]]; then
        echo -e "  ''${_green}PASS''${_reset}: $msg (''${time_ms}ms)"
      else
        echo -e "  ''${_green}PASS''${_reset}: $msg"
      fi
    }

    result_fail() {
      local msg="$1"
      local time_ms="''${2:-}"
      if [[ -n "$time_ms" ]]; then
        echo -e "  ''${_red}FAIL''${_reset}: $msg (''${time_ms}ms)"
      else
        echo -e "  ''${_red}FAIL''${_reset}: $msg"
      fi
    }

    result_warn() {
      local msg="$1"
      local time_ms="''${2:-}"
      if [[ -n "$time_ms" ]]; then
        echo -e "  ''${_yellow}WARN''${_reset}: $msg (''${time_ms}ms)"
      else
        echo -e "  ''${_yellow}WARN''${_reset}: $msg"
      fi
    }
  '';

  # ─── Timing Helpers ─────────────────────────────────────────────────────
  # Millisecond timing for phase durations (reused from lifecycle/lib.nix)
  timingHelpers = ''
    # Get current time in milliseconds
    time_ms() {
      echo $(($(date +%s%N) / 1000000))
    }

    # Calculate elapsed time in milliseconds
    elapsed_ms() {
      local start="$1"
      local now
      now=$(time_ms)
      echo $((now - start))
    }

    # Convert milliseconds to human-readable format
    format_ms() {
      local ms="$1"
      if [[ $ms -lt 1000 ]]; then
        echo "''${ms}ms"
      elif [[ $ms -lt 60000 ]]; then
        echo "$((ms / 1000)).$((ms % 1000 / 100))s"
      else
        local mins=$((ms / 60000))
        local secs=$(((ms % 60000) / 1000))
        echo "''${mins}m''${secs}s"
      fi
    }
  '';

  # ─── Container Runtime Helpers ──────────────────────────────────────────
  # Auto-detect and use docker or podman
  containerHelpers = ''
    CONTAINER_RUNTIME=""
    CONTAINER_IP=""

    # Detect available container runtime
    detect_runtime() {
      if command -v docker &>/dev/null && docker info &>/dev/null; then
        CONTAINER_RUNTIME="docker"
      elif command -v podman &>/dev/null; then
        CONTAINER_RUNTIME="podman"
      else
        error "No container runtime found (docker/podman)"
        error "Install docker or podman and ensure the daemon is running"
        exit 1
      fi
      info "Using container runtime: $CONTAINER_RUNTIME"
    }

    # Get container IP address (for direct connection, bypassing port mapping)
    get_container_ip() {
      local name="$1"
      $CONTAINER_RUNTIME inspect "$name" --format '{{range .NetworkSettings.Networks}}{{.IPAddress}}{{end}}' 2>/dev/null
    }

    # Check if container exists (running or stopped)
    container_exists() {
      local name="$1"
      $CONTAINER_RUNTIME ps -a --format '{{.Names}}' 2>/dev/null | grep -q "^''${name}$"
    }

    # Check if container is running
    container_running() {
      local name="$1"
      $CONTAINER_RUNTIME ps --format '{{.Names}}' 2>/dev/null | grep -q "^''${name}$"
    }

    # Execute command inside container
    container_exec() {
      $CONTAINER_RUNTIME exec "$CONTAINER_NAME" "$@"
    }

    # Remove container (force)
    container_remove() {
      local name="$1"
      $CONTAINER_RUNTIME rm -f "$name" &>/dev/null || true
    }

    # Stop container with timeout
    container_stop() {
      local name="$1"
      local timeout="$2"
      $CONTAINER_RUNTIME stop -t "$timeout" "$name" &>/dev/null
    }

    # Kill container
    container_kill() {
      local name="$1"
      $CONTAINER_RUNTIME kill "$name" &>/dev/null || true
    }
  '';

  # ─── Port Verification Helpers ──────────────────────────────────────────
  portHelpers = ''
    # Wait for a TCP port to be listening (uses container IP)
    wait_for_port() {
      local port="$1"
      local timeout="$2"
      local elapsed=0

      # Get container IP if not set
      if [[ -z "$CONTAINER_IP" ]]; then
        CONTAINER_IP=$(get_container_ip "$CONTAINER_NAME")
      fi

      while ! nc -z "$CONTAINER_IP" "$port" 2>/dev/null; do
        sleep 1
        elapsed=$((elapsed + 1))
        if [[ $elapsed -ge $timeout ]]; then
          return 1
        fi
      done
      return 0
    }

    # Check if port is listening (uses container IP)
    port_is_open() {
      local port="$1"

      # Get container IP if not set
      if [[ -z "$CONTAINER_IP" ]]; then
        CONTAINER_IP=$(get_container_ip "$CONTAINER_NAME")
      fi

      nc -z "$CONTAINER_IP" "$port" 2>/dev/null
    }
  '';

  # ─── Process Verification Helpers ───────────────────────────────────────
  processHelpers = ''
    # Check if a process is running inside the container
    # Uses /proc/1/comm since the container runs pmcd as PID 1
    check_process_in_container() {
      local proc="$1"
      local comm
      comm=$(container_exec cat /proc/1/comm 2>/dev/null || true)
      [[ "$comm" == "$proc" ]]
    }

    # Wait for process to appear in container
    wait_for_process_in_container() {
      local proc="$1"
      local timeout="$2"
      local elapsed=0

      while ! check_process_in_container "$proc"; do
        sleep 1
        elapsed=$((elapsed + 1))
        if [[ $elapsed -ge $timeout ]]; then
          return 1
        fi
      done
      return 0
    }
  '';

  # ─── Metric Verification Helpers ────────────────────────────────────────
  metricHelpers = ''
    # Check if a metric is available via pminfo (uses container IP)
    check_metric() {
      local metric="$1"

      # Get container IP if not set
      if [[ -z "$CONTAINER_IP" ]]; then
        CONTAINER_IP=$(get_container_ip "$CONTAINER_NAME")
      fi

      pminfo -h "$CONTAINER_IP" -f "$metric" &>/dev/null
    }

    # Verify all metrics in a space-separated list
    verify_all_metrics() {
      local metrics="$1"
      local failed=0

      for m in $metrics; do
        if check_metric "$m"; then
          result_pass "$m"
        else
          result_fail "$m"
          failed=$((failed + 1))
        fi
      done

      return $failed
    }
  '';

  # ─── Combined Helpers ───────────────────────────────────────────────────
  # All helpers combined for use in test scripts
  allHelpers = lib.concatStringsSep "\n" [
    colorHelpers
    timingHelpers
    containerHelpers
    portHelpers
    processHelpers
    metricHelpers
  ];
}
