# nix/lifecycle/lib.nix
#
# Script generators for PCP MicroVM lifecycle testing.
# Provides functions to generate bash scripts for each lifecycle phase.
#
# Adapted from xdp2's lifecycle testing framework with PCP-specific
# verification phases and variant handling.
#
{ pkgs, lib }:
let
  constants = import ./constants.nix { };
  mainConstants = import ../constants.nix;

  # Common runtime inputs for all lifecycle scripts
  commonInputs = with pkgs; [
    coreutils
    gnugrep
    gnused
    gawk
    procps
    netcat-gnu
    socat
    expect
    bc
    util-linux  # for kill, etc.
  ];

  # SSH-related inputs (for service verification via SSH fallback)
  sshInputs = with pkgs; [
    openssh
    sshpass
  ];

  # ANSI color helpers (shell functions)
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
      local time_ms="$2"
      echo -e "  ''${_green}PASS''${_reset}: $msg (''${time_ms}ms)"
    }

    result_fail() {
      local msg="$1"
      local time_ms="$2"
      echo -e "  ''${_red}FAIL''${_reset}: $msg (''${time_ms}ms)"
    }
  '';

  # Timing helpers
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

  # Process detection helpers
  processHelpers = ''
    # Check if VM process is running by hostname pattern
    # Returns 0 if running, 1 if not
    vm_is_running() {
      local hostname="$1"
      pgrep -f "process=$hostname" >/dev/null 2>&1
    }

    # Get VM process PID
    vm_pid() {
      local hostname="$1"
      pgrep -f "process=$hostname" 2>/dev/null | head -1
    }

    # Wait for VM process to start
    wait_for_process() {
      local hostname="$1"
      local timeout="$2"
      local poll_interval="${toString constants.lifecycle.pollInterval}"
      local elapsed=0

      while [[ $elapsed -lt $timeout ]]; do
        if vm_is_running "$hostname"; then
          return 0
        fi
        sleep "$poll_interval"
        elapsed=$((elapsed + poll_interval))
      done
      return 1
    }

    # Wait for VM process to exit
    wait_for_exit() {
      local hostname="$1"
      local timeout="$2"
      local poll_interval="${toString constants.lifecycle.pollInterval}"
      local elapsed=0

      while [[ $elapsed -lt $timeout ]]; do
        if ! vm_is_running "$hostname"; then
          return 0
        fi
        sleep "$poll_interval"
        elapsed=$((elapsed + poll_interval))
      done
      return 1
    }

    # Force kill VM process
    kill_vm() {
      local hostname="$1"
      local pid
      pid=$(vm_pid "$hostname")
      if [[ -n "$pid" ]]; then
        kill "$pid" 2>/dev/null || true
        sleep 1
        if vm_is_running "$hostname"; then
          kill -9 "$pid" 2>/dev/null || true
        fi
      fi
    }
  '';

  # Console connection helpers
  consoleHelpers = ''
    # Check if a TCP port is listening
    port_is_open() {
      local host="$1"
      local port="$2"
      nc -z "$host" "$port" 2>/dev/null
    }

    # Wait for console port to be available
    wait_for_console() {
      local port="$1"
      local timeout="$2"
      local poll_interval="${toString constants.lifecycle.pollInterval}"
      local elapsed=0

      while [[ $elapsed -lt $timeout ]]; do
        if port_is_open "127.0.0.1" "$port"; then
          return 0
        fi
        sleep "$poll_interval"
        elapsed=$((elapsed + poll_interval))
      done
      return 1
    }

    # Send command to console via expect and capture output
    # Usage: console_cmd <port> <command> <expect_pattern> <timeout>
    console_cmd() {
      local port="$1"
      local cmd="$2"
      local expect_pattern="$3"
      local timeout="''${4:-30}"

      expect -c "
        set timeout $timeout
        spawn socat -,rawer tcp:127.0.0.1:$port
        sleep 0.5
        send \"\\r\"
        expect {
          -re \"$expect_pattern\" {
            send \"$cmd\\r\"
            expect -re \"$expect_pattern\"
            exit 0
          }
          timeout {
            exit 1
          }
        }
      " 2>/dev/null
    }

    # Login to console and run a command
    console_login_cmd() {
      local port="$1"
      local username="$2"
      local password="$3"
      local cmd="$4"
      local timeout="''${5:-60}"

      expect -c "
        set timeout $timeout
        spawn socat -,rawer tcp:127.0.0.1:$port
        sleep 0.5
        send \"\\r\"
        expect {
          \"login:\" {
            send \"$username\\r\"
            expect \"Password:\"
            send \"$password\\r\"
            expect -re \"$username@.*:.*[#\$]\"
            send \"$cmd\\r\"
            expect -re \"$username@.*:.*[#\$]\"
            exit 0
          }
          -re \"$username@.*:.*[#\$]\" {
            # Already logged in
            send \"$cmd\\r\"
            expect -re \"$username@.*:.*[#\$]\"
            exit 0
          }
          timeout {
            exit 1
          }
        }
      " 2>/dev/null
    }
  '';

in
{
  inherit constants mainConstants commonInputs sshInputs;
  inherit colorHelpers timingHelpers processHelpers consoleHelpers;

  # Generate a polling script that waits for a condition
  # condition is a shell command that returns 0 when ready
  mkPollingScript = { name, condition, timeout, pollInterval ? 1, description ? "" }:
    ''
      ${timingHelpers}

      poll_until() {
        local timeout="$1"
        local poll_interval="$2"
        local start_time
        start_time=$(time_ms)
        local elapsed=0

        while [[ $elapsed -lt $timeout ]]; do
          if ${condition}; then
            echo "$(elapsed_ms "$start_time")"
            return 0
          fi
          sleep "$poll_interval"
          elapsed=$((elapsed + poll_interval))
        done
        echo "$(elapsed_ms "$start_time")"
        return 1
      }

      poll_until "${toString timeout}" "${toString pollInterval}"
    '';

  # Generate a script that checks VM process status
  mkCheckProcessScript = { variant }:
    let
      hostname = mainConstants.getHostname variant;
      timeout = mainConstants.getTimeout variant "processStart";
    in
    pkgs.writeShellApplication {
      name = "pcp-lifecycle-1-check-process-${variant}";
      runtimeInputs = commonInputs;
      text = ''
        ${colorHelpers}
        ${timingHelpers}
        ${processHelpers}

        HOSTNAME="${hostname}"
        TIMEOUT=${toString timeout}

        phase_header "1" "Check Process" "$TIMEOUT"

        start_time=$(time_ms)
        if wait_for_process "$HOSTNAME" "$TIMEOUT"; then
          elapsed=$(elapsed_ms "$start_time")
          pid=$(vm_pid "$HOSTNAME")
          result_pass "VM process '$HOSTNAME' running (PID: $pid)" "$elapsed"
          exit 0
        else
          elapsed=$(elapsed_ms "$start_time")
          result_fail "VM process '$HOSTNAME' not found" "$elapsed"
          exit 1
        fi
      '';
    };

  # Generate a script that checks serial console availability
  mkCheckSerialScript = { variant }:
    let
      consolePorts = mainConstants.getConsolePorts variant;
      timeout = mainConstants.getTimeout variant "serialReady";
    in
    pkgs.writeShellApplication {
      name = "pcp-lifecycle-2-check-serial-${variant}";
      runtimeInputs = commonInputs;
      text = ''
        ${colorHelpers}
        ${timingHelpers}
        ${consoleHelpers}

        SERIAL_PORT=${toString consolePorts.serial}
        TIMEOUT=${toString timeout}

        phase_header "2" "Check Serial Console" "$TIMEOUT"

        start_time=$(time_ms)
        if wait_for_console "$SERIAL_PORT" "$TIMEOUT"; then
          elapsed=$(elapsed_ms "$start_time")
          result_pass "Serial console available on port $SERIAL_PORT" "$elapsed"
          exit 0
        else
          elapsed=$(elapsed_ms "$start_time")
          result_fail "Serial console not available on port $SERIAL_PORT" "$elapsed"
          exit 1
        fi
      '';
    };

  # Generate a script that checks virtio console availability
  mkCheckVirtioScript = { variant }:
    let
      consolePorts = mainConstants.getConsolePorts variant;
      timeout = mainConstants.getTimeout variant "virtioReady";
    in
    pkgs.writeShellApplication {
      name = "pcp-lifecycle-2b-check-virtio-${variant}";
      runtimeInputs = commonInputs;
      text = ''
        ${colorHelpers}
        ${timingHelpers}
        ${consoleHelpers}

        VIRTIO_PORT=${toString consolePorts.virtio}
        TIMEOUT=${toString timeout}

        phase_header "2b" "Check Virtio Console" "$TIMEOUT"

        start_time=$(time_ms)
        if wait_for_console "$VIRTIO_PORT" "$TIMEOUT"; then
          elapsed=$(elapsed_ms "$start_time")
          result_pass "Virtio console available on port $VIRTIO_PORT" "$elapsed"
          exit 0
        else
          elapsed=$(elapsed_ms "$start_time")
          result_fail "Virtio console not available on port $VIRTIO_PORT" "$elapsed"
          exit 1
        fi
      '';
    };

  # Generate a shutdown script
  mkShutdownScript = { variant }:
    let
      consolePorts = mainConstants.getConsolePorts variant;
      hostname = mainConstants.getHostname variant;
      timeout = mainConstants.getTimeout variant "shutdown";
    in
    pkgs.writeShellApplication {
      name = "pcp-lifecycle-5-shutdown-${variant}";
      runtimeInputs = commonInputs;
      text = ''
        ${colorHelpers}
        ${timingHelpers}
        ${consoleHelpers}
        ${processHelpers}

        VIRTIO_PORT=${toString consolePorts.virtio}
        HOSTNAME="${hostname}"
        TIMEOUT=${toString timeout}

        phase_header "5" "Shutdown" "$TIMEOUT"

        start_time=$(time_ms)

        # Try to send shutdown command via virtio console
        if port_is_open "127.0.0.1" "$VIRTIO_PORT"; then
          info "  Sending shutdown command via virtio console..."
          if console_login_cmd "$VIRTIO_PORT" "root" "pcp" "poweroff" "$TIMEOUT"; then
            elapsed=$(elapsed_ms "$start_time")
            result_pass "Shutdown command sent" "$elapsed"
            exit 0
          fi
        fi

        # Fallback: kill the process
        warn "  Console shutdown failed, killing process..."
        kill_vm "$HOSTNAME"
        elapsed=$(elapsed_ms "$start_time")
        result_pass "VM process killed" "$elapsed"
      '';
    };

  # Generate a wait-for-exit script
  mkWaitExitScript = { variant }:
    let
      hostname = mainConstants.getHostname variant;
      timeout = mainConstants.getTimeout variant "waitExit";
    in
    pkgs.writeShellApplication {
      name = "pcp-lifecycle-6-wait-exit-${variant}";
      runtimeInputs = commonInputs;
      text = ''
        ${colorHelpers}
        ${timingHelpers}
        ${processHelpers}

        HOSTNAME="${hostname}"
        TIMEOUT=${toString timeout}

        phase_header "6" "Wait for Exit" "$TIMEOUT"

        start_time=$(time_ms)
        if wait_for_exit "$HOSTNAME" "$TIMEOUT"; then
          elapsed=$(elapsed_ms "$start_time")
          result_pass "VM exited cleanly" "$elapsed"
          exit 0
        else
          elapsed=$(elapsed_ms "$start_time")
          result_fail "VM did not exit within timeout, forcing kill" "$elapsed"
          kill_vm "$HOSTNAME"
          exit 1
        fi
      '';
    };

  # Generate a force-kill script
  mkForceKillScript = { variant }:
    let
      hostname = mainConstants.getHostname variant;
    in
    pkgs.writeShellApplication {
      name = "pcp-lifecycle-force-kill-${variant}";
      runtimeInputs = commonInputs;
      text = ''
        ${colorHelpers}
        ${processHelpers}

        HOSTNAME="${hostname}"

        info "Force killing VM: $HOSTNAME"

        if vm_is_running "$HOSTNAME"; then
          kill_vm "$HOSTNAME"
          if vm_is_running "$HOSTNAME"; then
            error "Failed to kill VM process"
            exit 1
          else
            success "VM process killed"
          fi
        else
          info "VM process not running"
        fi
      '';
    };

  # Generate a status script
  mkStatusScript = { variant }:
    let
      hostname = mainConstants.getHostname variant;
      consolePorts = mainConstants.getConsolePorts variant;
      portOffset = mainConstants.variantPortOffsets.${variant};
    in
    pkgs.writeShellApplication {
      name = "pcp-lifecycle-status-${variant}";
      runtimeInputs = commonInputs;
      text = ''
        ${colorHelpers}
        ${processHelpers}
        ${consoleHelpers}

        HOSTNAME="${hostname}"
        SERIAL_PORT=${toString consolePorts.serial}
        VIRTIO_PORT=${toString consolePorts.virtio}
        SSH_PORT=$((${toString mainConstants.ports.sshForward} + ${toString portOffset}))

        bold "MicroVM Status: ${variant}"
        echo ""

        # Process status
        if vm_is_running "$HOSTNAME"; then
          pid=$(vm_pid "$HOSTNAME")
          success "  Process: Running (PID: $pid)"
        else
          error "  Process: Not running"
        fi

        # Console status
        if port_is_open "127.0.0.1" "$SERIAL_PORT"; then
          success "  Serial Console: Available (port $SERIAL_PORT)"
        else
          warn "  Serial Console: Not available"
        fi

        if port_is_open "127.0.0.1" "$VIRTIO_PORT"; then
          success "  Virtio Console: Available (port $VIRTIO_PORT)"
        else
          warn "  Virtio Console: Not available"
        fi

        # SSH status
        if port_is_open "127.0.0.1" "$SSH_PORT"; then
          success "  SSH: Available (port $SSH_PORT)"
        else
          warn "  SSH: Not available (port $SSH_PORT)"
        fi
      '';
    };
}
