# nix/network-setup.nix
#
# TAP/bridge/vhost-net setup and teardown scripts.
# All network parameters come from constants.nix.
#
# Usage (setup and teardown require sudo):
#   nix run .#pcp-check-host             # Verify host environment
#   sudo nix run .#pcp-network-setup     # Create bridge + TAP + NAT
#   sudo nix run .#pcp-network-teardown  # Remove bridge + TAP + NAT
#
{ pkgs }:
let
  constants = import ./constants.nix;
  inherit (constants.network) bridge tap subnet gateway vmIp;
in
{
  # Host environment check
  # Verify the host has necessary kernel modules and devices before setup.
  check = pkgs.writeShellApplication {
    name = "pcp-check-host";
    runtimeInputs = with pkgs; [ kmod coreutils ];
    text = ''
      echo "=== PCP MicroVM Host Environment Check ==="
      errors=0

      # Check for TUN device
      if [[ -c /dev/net/tun ]]; then
        echo "OK /dev/net/tun exists"
      else
        echo "FAIL /dev/net/tun not found"
        echo "  Run: sudo modprobe tun"
        errors=$((errors + 1))
      fi

      # Check for vhost-net module/device
      if lsmod | grep -q vhost_net; then
        echo "OK vhost_net module loaded"
      elif [[ -c /dev/vhost-net ]]; then
        echo "OK /dev/vhost-net exists"
      else
        echo "FAIL vhost_net not available"
        echo "  Run: sudo modprobe vhost_net"
        errors=$((errors + 1))
      fi

      # Check for bridge module
      if lsmod | grep -q bridge; then
        echo "OK bridge module loaded"
      else
        echo "INFO bridge module not loaded (will be loaded during setup)"
      fi

      # Check sudo access
      if sudo -n true 2>/dev/null; then
        echo "OK sudo access available"
      else
        echo "FAIL sudo access required for network setup"
        errors=$((errors + 1))
      fi

      if [[ $errors -gt 0 ]]; then
        echo ""
        echo "Host environment check failed with $errors error(s)"
        exit 1
      else
        echo ""
        echo "Host environment ready for TAP networking"
      fi
    '';
  };

  # Network setup
  # Create bridge, TAP device, and NAT rules for VM networking.
  setup = pkgs.writeShellApplication {
    name = "pcp-network-setup";
    runtimeInputs = with pkgs; [ iproute2 kmod nftables acl ];
    text = ''
      echo "=== PCP MicroVM Network Setup ==="

      # Check we're running as root (via sudo)
      if [[ $EUID -ne 0 ]]; then
        echo "ERROR: Run with sudo: sudo nix run .#pcp-network-setup"
        exit 1
      fi

      # Determine the actual user (not root when running via sudo)
      REAL_USER="''${SUDO_USER:-$USER}"
      if [[ "$REAL_USER" == "root" ]]; then
        echo "ERROR: Run this script via 'sudo nix run .#pcp-network-setup' as a regular user"
        echo "       The script needs to know which user should have TAP device access"
        exit 1
      fi
      echo "Setting up network for user: $REAL_USER"

      # Load required kernel modules
      modprobe tun
      modprobe vhost_net
      modprobe bridge

      # Create bridge
      if ! ip link show ${bridge} &>/dev/null; then
        echo "Creating bridge ${bridge}..."
        ip link add ${bridge} type bridge
        ip addr add ${gateway}/24 dev ${bridge}
        ip link set ${bridge} up
      else
        echo "Bridge ${bridge} already exists"
      fi

      # Create TAP device with multi_queue for vhost-net
      # Recreate if it exists but with wrong owner
      if ip link show ${tap} &>/dev/null; then
        echo "Removing existing TAP device ${tap}..."
        ip link del ${tap}
      fi
      echo "Creating TAP device ${tap} for user $REAL_USER..."
      ip tuntap add dev ${tap} mode tap multi_queue user "$REAL_USER"
      ip link set ${tap} master ${bridge}
      ip link set ${tap} up

      # Enable vhost-net access (secure method: ACL, fallback: group)
      # SECURITY: We avoid chmod 666 (world-writable) as it's a red flag
      if [[ -c /dev/vhost-net ]]; then
        if command -v setfacl &>/dev/null; then
          # Preferred: ACL-based per-user access
          setfacl -m "u:$REAL_USER:rw" /dev/vhost-net
          echo "vhost-net enabled (ACL for $REAL_USER)"
        elif getent group kvm &>/dev/null; then
          # Fallback: group-based access (user must be in kvm group)
          chgrp kvm /dev/vhost-net
          chmod 660 /dev/vhost-net
          echo "vhost-net enabled (kvm group)"
        else
          echo "WARNING: Cannot set vhost-net permissions securely"
          echo "  Option 1: Install acl package and rerun setup"
          echo "  Option 2: Add $REAL_USER to 'kvm' group and rerun setup"
          echo "  vhost acceleration may not work"
        fi
      fi

      # NAT for VM internet access
      echo "Configuring NAT..."
      nft add table inet pcp-nat 2>/dev/null || true
      nft flush table inet pcp-nat 2>/dev/null || true
      nft -f - <<EOF
table inet pcp-nat {
  chain postrouting {
    type nat hook postrouting priority 100;
    ip saddr ${subnet} masquerade
  }
  chain forward {
    type filter hook forward priority 0;
    iifname "${bridge}" accept
    oifname "${bridge}" ct state related,established accept
  }
}
EOF

      # Enable IP forwarding
      sysctl -w net.ipv4.ip_forward=1 >/dev/null

      echo ""
      echo "Network ready. MicroVM will be accessible at:"
      echo "  pmcd:           ${vmIp}:${toString constants.ports.pmcd}"
      echo "  pmproxy:        ${vmIp}:${toString constants.ports.pmproxy}"
      echo "  node_exporter:  ${vmIp}:${toString constants.ports.nodeExporter}"
      echo "  SSH:            ssh root@${vmIp}"
    '';
  };

  # Network teardown
  # Remove bridge, TAP device, and NAT rules.
  # Run with: sudo nix run .#pcp-network-teardown
  teardown = pkgs.writeShellApplication {
    name = "pcp-network-teardown";
    runtimeInputs = with pkgs; [ iproute2 nftables ];
    text = ''
      echo "=== PCP MicroVM Network Teardown ==="

      # Check we're running as root
      if [[ $EUID -ne 0 ]]; then
        echo "ERROR: Run with sudo: sudo nix run .#pcp-network-teardown"
        exit 1
      fi

      # Remove TAP device
      if ip link show ${tap} &>/dev/null; then
        ip link del ${tap}
        echo "Removed TAP device ${tap}"
      fi

      # Remove bridge
      if ip link show ${bridge} &>/dev/null; then
        ip link set ${bridge} down
        ip link del ${bridge}
        echo "Removed bridge ${bridge}"
      fi

      # Remove NAT rules
      nft delete table inet pcp-nat 2>/dev/null && \
        echo "Removed NAT rules" || true

      echo "Network teardown complete"
    '';
  };
}
