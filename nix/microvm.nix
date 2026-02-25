# nix/microvm.nix
#
# Parametric MicroVM generator for PCP.
# This single module handles all MicroVM configurations via parameters.
#
# Parameters:
#   networking      - "user" (port forwarding) or "tap" (direct network)
#   debugMode       - Enable password SSH (default: true for interactive testing)
#   enablePmlogger  - Enable archive logging (default: true)
#   enableEvalTools - Enable node_exporter + below for comparison (default: false)
#   enablePmieTest  - Enable stress-ng workload + pmie rules (default: false)
#   enableGrafana   - Enable Grafana + Prometheus for visual monitoring (default: false)
#   enableBpf       - Enable pre-compiled BPF PMDA (default: false)
#   enableBcc       - Enable runtime BCC PMDA, requires 2GB (default: false)
#
# Helper scripts (see nix/microvm-scripts.nix):
#   nix run .#pcp-vm-check  - List running PCP MicroVMs and show count
#   nix run .#pcp-vm-stop   - Stop all running PCP MicroVMs
#   nix run .#pcp-vm-ssh    - SSH into the VM as root (debug mode only)
#
# Security notes:
# - debugMode is enabled by default for interactive testing convenience
# - For production/CI use, set debugMode = false and configure SSH keys
#
# Returns the microvm.declaredRunner - a script that starts the VM.
#
{
  pkgs,
  lib,
  pcp,
  microvm,
  nixosModule,
  nixpkgs,
  system,
  networking ? "user",        # "user" or "tap"
  debugMode ? true,           # Enable password auth (default: true for interactive testing)
  enablePmlogger ? true,      # Archive logging
  enableEvalTools ? false,    # node_exporter, below
  enablePmieTest ? false,     # stress-ng + pmie rules
  enableGrafana ? false,      # Grafana + Prometheus
  enableBpf ? false,          # Pre-compiled CO-RE eBPF (lightweight)
  enableBcc ? false,          # Runtime eBPF compilation (heavyweight, 2GB)
  portOffset ? 0,             # Port offset for all forwarded ports (see constants.variantPortOffsets)
  variant ? "base",           # Variant name for console port allocation (base, eval, grafana, bpf, bcc)
}:
let
  constants = import ./constants.nix;
  useTap = networking == "tap";

  # Get serial console ports for this variant
  consolePorts = constants.getConsolePorts variant;

  # BCC overlay: change KERNEL_MODULES_DIR from /run/booted-system/... to /lib/modules
  # This is required because /run/booted-system is a symlink to read-only Nix store,
  # but we need to bind mount kernel dev headers at this location.
  # See bcc.nix for the bind mount configuration.
  bccOverlay = final: prev: {
    bcc = prev.bcc.overrideAttrs (old: {
      cmakeFlags = builtins.map (flag:
        if builtins.match ".*BCC_KERNEL_MODULES_DIR.*" flag != null
        then "-DBCC_KERNEL_MODULES_DIR=/lib/modules"
        else flag
      ) old.cmakeFlags;
    });
  };

  # Dynamic hostname based on enabled features
  # Priority: bcc > grafana > bpf > eval > base
  # Note: grafana comes before bpf because grafana variant enables bpf for dashboards
  #       but should still be identified as grafana-vm for lifecycle testing
  hostname =
    if enableBcc then "pcp-bcc-vm"
    else if enableGrafana then "pcp-grafana-vm"
    else if enableBpf then "pcp-bpf-vm"
    else if enableEvalTools || enablePmieTest then "pcp-eval-vm"
    else "pcp-vm";

  # Dynamic memory: 2GB+ for BCC (clang/LLVM), 1GB otherwise
  # Use 2049 instead of 2048 to avoid QEMU hang with exactly 2GB
  # See: https://github.com/microvm-nix/microvm.nix/issues/171
  memoryMB = if enableBcc then 2049 else constants.vm.memoryMB;

  # Build a NixOS system with MicroVM support
  vmConfig = nixpkgs.lib.nixosSystem {
    inherit system;

    # Pass PCP package into modules via specialArgs
    specialArgs = { inherit pcp; };

    modules = [
      # Import MicroVM NixOS module
      microvm.nixosModules.microvm

      # Import our PCP NixOS module
      nixosModule

      # Import pmie testing module (stress-ng workload + pmie rules)
      ./pmie-test.nix

      # Import Grafana module (provides services.pcp.grafana option)
      ./grafana.nix

      # Import BPF module (provides services.pcp.bpf option)
      ./bpf.nix

      # Import BCC module (provides services.pcp.bcc option)
      ./bcc.nix

      # PCP service configuration
      ({ pcp, ... }: {
        services.pcp = {
          enable = true;
          package = pcp;
          preset = "custom";
          pmlogger.enable = enablePmlogger;
          pmie.enable = false;
          pmproxy.enable = true;
          # For TAP networking, restrict to bridge subnet
          allowedNetworks = lib.optionals useTap [
            constants.network.subnet
            "127.0.0.0/8"
            "::1/128"
          ];
        };
      })

      # MicroVM and system configuration
      ({ config, pkgs, ... }: {
        system.stateVersion = "26.05";

        nixpkgs.hostPlatform = system;

        # Apply BCC overlay when BCC is enabled
        # This changes BCC's KERNEL_MODULES_DIR to /lib/modules (see bccOverlay above)
        nixpkgs.overlays = lib.optionals enableBcc [ bccOverlay ];

        microvm = {
          hypervisor = "qemu";
          mem = memoryMB;
          vcpu = constants.vm.vcpus;

          # Share host's nix store read-only
          shares = [{
            tag = "ro-store";
            source = "/nix/store";
            mountPoint = "/nix/.ro-store";
            proto = "9p";
          }];

          # Network interface configuration
          interfaces = if useTap then [{
            type = "tap";
            id = constants.network.tap;
            mac = constants.network.vmMac;
          }] else [{
            type = "user";
            id = "eth0";
            mac = constants.network.vmMac;
          }];

          # Port forwarding for user-mode networking (additive based on features)
          # All host ports are shifted by portOffset to allow multiple VMs to coexist
          forwardPorts = lib.optionals (!useTap) (
            # Base ports: pmcd, pmproxy, ssh
            [
              { from = "host"; host.port = constants.ports.pmcd + portOffset; guest.port = constants.ports.pmcd; }
              { from = "host"; host.port = constants.ports.pmproxy + portOffset; guest.port = constants.ports.pmproxy; }
              { from = "host"; host.port = constants.ports.sshForward + portOffset; guest.port = 22; }
            ]
            # Eval tools: node_exporter
            ++ lib.optionals enableEvalTools [
              { from = "host"; host.port = constants.ports.nodeExporter + portOffset; guest.port = constants.ports.nodeExporter; }
            ]
            # Grafana: Grafana + Prometheus
            ++ lib.optionals enableGrafana [
              { from = "host"; host.port = constants.ports.grafanaForward + portOffset; guest.port = constants.ports.grafana; }
              { from = "host"; host.port = constants.ports.prometheusForward + portOffset; guest.port = constants.ports.prometheus; }
            ]
          );

          # ─── Serial Console Configuration ─────────────────────────────────
          # Two consoles for debugging boot issues and network problems:
          #   - ttyS0 (serial): Slow but available immediately at boot
          #   - hvc0 (virtio):  Fast, available after virtio drivers load
          #
          # Connect with: nc localhost <port>
          # Or for raw mode: socat -,rawer tcp:localhost:<port>
          #
          qemu = {
            # Disable default serial console (we configure our own TCP-accessible ones)
            serialConsole = false;

            extraArgs = [
              # VM identification (for ps/pgrep matching)
              "-name" "${hostname},process=${hostname}"

              # Serial console on TCP (ttyS0) - slow but early boot access
              "-serial" "tcp:127.0.0.1:${toString consolePorts.serial},server,nowait"

              # Virtio console (hvc0) - high-speed, requires drivers
              "-device" "virtio-serial-pci"
              "-chardev" "socket,id=virtcon,port=${toString consolePorts.virtio},host=127.0.0.1,server=on,wait=off"
              "-device" "virtconsole,chardev=virtcon"
            ];
          };
        };

        # Console output configuration - send to both serial and virtio
        boot.kernelParams = [
          "console=ttyS0,115200"  # Serial first (early boot messages)
          "console=hvc0"          # Virtio second (becomes primary after driver loads)
        ];

        networking.hostName = hostname;

        # Static IP for TAP networking
        # Use systemd-networkd for reliable interface matching (enp* covers PCI ethernet)
        systemd.network = lib.mkIf useTap {
          enable = true;
          networks."10-tap" = {
            matchConfig.Name = "enp*";
            networkConfig = {
              Address = "${constants.network.vmIp}/24";
              Gateway = constants.network.gateway;
              DHCP = "no";
            };
          };
        };
        # Disable dhcpcd to avoid conflicts with systemd-networkd
        networking.useDHCP = lib.mkIf useTap false;

        # SSH Configuration
        # Default: password auth enabled for interactive testing convenience
        services.openssh = {
          enable = true;
          settings = {
            PasswordAuthentication = false;
            PermitRootLogin = "prohibit-password";
            KbdInteractiveAuthentication = false;
          };
        };

        # Create a pcp-admin user for SSH key access
        users.users.pcp-admin = {
          isNormalUser = true;
          extraGroups = [ "wheel" ];
          openssh.authorizedKeys.keys = [
            # Users should add their keys here or via module option
          ];
        };

        # Allow passwordless sudo for pcp-admin
        security.sudo.wheelNeedsPassword = false;
      })

      # ─── pmie Testing Module ───────────────────────────────────────────
      # Enables stress-ng workload and pmie rules for testing the inference engine.
      (lib.mkIf enablePmieTest {
        services.pcp.pmieTest.enable = true;
      })

      # ─── Evaluation Tools ──────────────────────────────────────────────
      # node_exporter and below for metric comparison
      ({ pkgs, ... }: lib.mkIf enableEvalTools {
        # Prometheus node_exporter for comparison with PCP metrics
        services.prometheus.exporters.node = {
          enable = true;
          port = constants.ports.nodeExporter;
          enabledCollectors = [
            "cpu" "diskstats" "filesystem" "loadavg"
            "meminfo" "netdev" "netstat" "stat" "time" "vmstat"
          ];
          disabledCollectors = [ "textfile" ];
        };

        # below - Meta's time-traveling resource monitor
        environment.systemPackages = [ pkgs.below ];
        services.below.enable = true;

        # Open firewall for node_exporter
        networking.firewall.allowedTCPPorts = [ constants.ports.nodeExporter ];
      })

      # ─── Grafana Module ─────────────────────────────────────────────────
      # Enables Grafana with PCP Vector dashboards for visual monitoring.
      (lib.mkIf enableGrafana {
        # Enable Grafana with PCP dashboards
        services.pcp.grafana.enable = true;

        # Prometheus server for comparison with PCP
        services.prometheus = {
          enable = true;
          port = constants.ports.prometheus;

          # Minimal retention for eval VM
          retentionTime = "1d";

          # Scrape node_exporter (if enabled)
          scrapeConfigs = lib.optionals enableEvalTools [{
            job_name = "node";
            static_configs = [{
              targets = [ "localhost:${toString constants.ports.nodeExporter}" ];
            }];
            scrape_interval = "15s";
          }];
        };

        # Open firewall for Prometheus
        networking.firewall.allowedTCPPorts = [ constants.ports.prometheus ];
      })

      # ─── BPF PMDA (Pre-compiled eBPF) ────────────────────────────────
      # Pre-compiled CO-RE eBPF: fast startup, low memory.
      # Metrics: bpf.runqlat, bpf.biolatency, bpf.netatop, bpf.oomkill
      (lib.mkIf enableBpf {
        services.pcp.bpf = {
          enable = true;
        };

        # BPF PMDA requires BTF for CO-RE relocation
        boot.kernelPatches = [
          {
            name = "btf-for-bpf";
            patch = null;
            structuredExtraConfig = with lib.kernel; {
              DEBUG_INFO_BTF = yes;
            };
          }
        ];
      })

      # ─── BCC PMDA (Runtime-compiled eBPF) ───────────────────────────
      # Runtime eBPF compilation: slow startup (~30-60s), 2GB memory.
      # Required for: tcptop, tcplife metrics (Grafana eBPF/BCC Overview dashboard)
      (lib.mkIf enableBcc {
        services.pcp.bcc = {
          enable = true;
          moduleFailureFatal = false;  # Continue if some modules fail to compile
        };

        boot.kernelPatches = [
          {
            name = "btf-for-bcc";
            patch = null;
            structuredExtraConfig = with lib.kernel; {
              DEBUG_INFO_BTF = yes;
            };
          }
        ];
      })

      # ─── Debug Mode Module ────────────────────────────────────────────
      # Enables password auth for quick local testing.
      (lib.mkIf debugMode {
        warnings = [
          "PCP MicroVM is running in DEBUG MODE with insecure SSH settings!"
        ];

        services.openssh.settings = {
          PasswordAuthentication = lib.mkForce true;
          PermitRootLogin = lib.mkForce "yes";
          KbdInteractiveAuthentication = lib.mkForce true;
        };

        users.users.root.password = "pcp";

        environment.etc."motd".text = ''
          ${lib.optionalString enableBcc ''
          ╔═══════════════════════════════════════════════════════════════╗
          ║  PCP MicroVM with BCC PMDA (runtime eBPF compilation)         ║
          ╠═══════════════════════════════════════════════════════════════╣
          ║  BCC modules take 30-60s to compile at pmcd startup.          ║
          ║  Check status: journalctl -u pmcd -f                          ║
          ╚═══════════════════════════════════════════════════════════════╝
          ''}${lib.optionalString enableBpf ''
          ╔═══════════════════════════════════════════════════════════════╗
          ║  PCP MicroVM with BPF PMDA (pre-compiled eBPF)                ║
          ╠═══════════════════════════════════════════════════════════════╣
          ║  Query eBPF metrics: pminfo -f bpf.runqlat.latency            ║
          ╚═══════════════════════════════════════════════════════════════╝
          ''}${lib.optionalString enableGrafana ''
          ╔═══════════════════════════════════════════════════════════════╗
          ║  PCP MicroVM with Grafana                                     ║
          ╠═══════════════════════════════════════════════════════════════╣
          ║  Grafana:    http://localhost:${toString constants.ports.grafanaForward} (admin/pcp)     ║
          ║  Prometheus: http://localhost:${toString constants.ports.prometheusForward}                    ║
          ╚═══════════════════════════════════════════════════════════════╝
          ''}${lib.optionalString (enableEvalTools && !enableGrafana && !enableBpf && !enableBcc) ''
          ╔═══════════════════════════════════════════════════════════════╗
          ║  PCP Evaluation MicroVM                                       ║
          ╠═══════════════════════════════════════════════════════════════╣
          ║  PCP:            pminfo -f kernel.all.load                    ║
          ║  node_exporter:  curl localhost:9100/metrics                  ║
          ║  below:          below live                                   ║
          ╚═══════════════════════════════════════════════════════════════╝
          ''}${lib.optionalString (!enableEvalTools && !enableGrafana && !enableBpf && !enableBcc) ''
          ╔═══════════════════════════════════════════════════════════════╗
          ║  PCP Base MicroVM                                             ║
          ╠═══════════════════════════════════════════════════════════════╣
          ║  Query metrics: pminfo -f kernel.all.load                     ║
          ║  pmlogger archives: /var/log/pcp/pmlogger                     ║
          ╚═══════════════════════════════════════════════════════════════╝
          ''}
          WARNING: DEBUG MODE - Password authentication is enabled.
          Do NOT use in production.
        '';
      })
    ];
  };
in
# Return the runner script that starts this MicroVM
vmConfig.config.microvm.declaredRunner
