# nix/microvm-scripts.nix
#
# Helper scripts for managing PCP MicroVMs.
# These provide simple Unix-idiomatic ways to check, stop, and connect to VMs.
#
# Port offsets per variant (see constants.variantPortOffsets):
#   base:    0   -> SSH port 22022
#   eval:    100 -> SSH port 22122
#   grafana: 200 -> SSH port 22222
#   bpf:     300 -> SSH port 22322
#   bcc:     400 -> SSH port 22422
#
{ pkgs }:
let
  constants = import ./constants.nix;
  baseSshPort = constants.ports.sshForward;

  # Pattern to identify our MicroVMs in process list
  # Hostnames: pcp-vm, pcp-eval-vm, pcp-grafana-vm, pcp-bpf-vm, pcp-bcc-vm
  vmPattern = "microvm@pcp-(vm|eval-vm|grafana-vm|bpf-vm|bcc-vm)";

in {
  # ─── Check Script ───────────────────────────────────────────────────────────
  # Lists running PCP MicroVM processes and shows a count.
  #
  # Usage: nix run .#pcp-vm-check
  #
  check = pkgs.writeShellApplication {
    name = "pcp-vm-check";
    runtimeInputs = with pkgs; [ procps ];
    text = ''
      echo "=== PCP MicroVM Processes ==="
      echo

      # Use pgrep -af to show full command line (-f matches full cmdline)
      if pgrep -af '${vmPattern}'; then
        echo
        echo "=== Count ==="
        pgrep -cf '${vmPattern}'
      else
        echo "(none running)"
        echo
        echo "=== Count ==="
        echo "0"
      fi
    '';
  };

  # ─── Stop Script ────────────────────────────────────────────────────────────
  # Kills all running PCP MicroVM processes.
  #
  # Usage: nix run .#pcp-vm-stop
  #
  stop = pkgs.writeShellApplication {
    name = "pcp-vm-stop";
    runtimeInputs = with pkgs; [ procps ];
    text = ''
      echo "=== Stopping PCP MicroVMs ==="

      # Check if any are running
      if ! pgrep -f '${vmPattern}' > /dev/null; then
        echo "No PCP MicroVMs running."
        exit 0
      fi

      echo "Found processes:"
      pgrep -af '${vmPattern}'

      echo
      echo "Sending SIGTERM..."
      pkill -f '${vmPattern}' || true

      sleep 1

      # Check if any survived
      if pgrep -f '${vmPattern}' > /dev/null; then
        echo "Processes still running, sending SIGKILL..."
        pkill -9 -f '${vmPattern}' || true
      fi

      echo "Done."
    '';
  };

  # ─── SSH Script ─────────────────────────────────────────────────────────────
  # Connects to the MicroVM as root via SSH.
  # Uses password auth (debug mode only - password is "pcp").
  #
  # Usage:
  #   nix run .#pcp-vm-ssh                    # Connect to base variant (port 22022)
  #   nix run .#pcp-vm-ssh -- --variant=eval  # Connect to eval variant (port 22122)
  #   nix run .#pcp-vm-ssh -- -p 22222        # Connect to specific port
  #
  ssh = pkgs.writeShellApplication {
    name = "pcp-vm-ssh";
    runtimeInputs = with pkgs; [ openssh sshpass ];
    text = ''
      # Disable SSH agent to avoid keyring popups
      unset SSH_AUTH_SOCK

      # Default port (base variant)
      PORT=${toString baseSshPort}

      # Parse arguments
      PASSTHROUGH_ARGS=()
      while [[ $# -gt 0 ]]; do
        case "$1" in
          --variant=*)
            VARIANT="''${1#--variant=}"
            case "$VARIANT" in
              base)    PORT=$((${toString baseSshPort} + ${toString constants.variantPortOffsets.base})) ;;
              eval)    PORT=$((${toString baseSshPort} + ${toString constants.variantPortOffsets.eval})) ;;
              grafana) PORT=$((${toString baseSshPort} + ${toString constants.variantPortOffsets.grafana})) ;;
              bpf)     PORT=$((${toString baseSshPort} + ${toString constants.variantPortOffsets.bpf})) ;;
              bcc)     PORT=$((${toString baseSshPort} + ${toString constants.variantPortOffsets.bcc})) ;;
              *)
                echo "Unknown variant: $VARIANT"
                echo "Valid variants: base, eval, grafana, bpf, bcc"
                exit 1
                ;;
            esac
            shift
            ;;
          -p)
            # User specified port directly, use it
            PORT="$2"
            shift 2
            ;;
          *)
            PASSTHROUGH_ARGS+=("$1")
            shift
            ;;
        esac
      done

      # Connect with password "pcp" (debug mode)
      exec sshpass -p pcp ssh \
        -o StrictHostKeyChecking=no \
        -o UserKnownHostsFile=/dev/null \
        -o LogLevel=ERROR \
        -p "$PORT" \
        root@localhost "''${PASSTHROUGH_ARGS[@]}"
    '';
  };
}
