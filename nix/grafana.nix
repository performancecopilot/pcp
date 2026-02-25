# nix/grafana.nix
#
# NixOS module for Grafana with PCP dashboards.
# Provides visualization for PCP metrics via the grafana-pcp plugin.
#
# When Prometheus is also enabled, provisions the Node Exporter Full
# dashboard for head-to-head comparison of PCP vs Prometheus metrics.
#
# Usage:
#   imports = [ ./grafana.nix ];
#   services.pcp.grafana.enable = true;
#
{ config, lib, pkgs, ... }:

with lib;
let
  cfg = config.services.pcp.grafana;
  pcpCfg = config.services.pcp;
  constants = import ./constants.nix;

  # ─── grafana-pcp plugin ───────────────────────────────────────────────
  # Fetched from Grafana CDN and unpacked.
  # Version 5.0.0 is the current stable release (2022-06-30).
  # Note: In v5.0.0 the datasource is called "redis", renamed to "valkey" in later versions.
  pcpPlugin = pkgs.stdenv.mkDerivation {
    pname = "grafana-pcp-plugin";
    version = "5.0.0";

    src = pkgs.fetchurl {
      url = "https://grafana.com/api/plugins/performancecopilot-pcp-app/versions/5.0.0/download";
      sha256 = "0jgdnzzinv2skw7rxkijkgkjw4aal2znqpkn2xczlksh9xymfmvj";
      name = "grafana-pcp-5.0.0.zip";
    };

    nativeBuildInputs = [ pkgs.unzip ];

    unpackPhase = ''
      runHook preUnpack
      unzip $src
      runHook postUnpack
    '';

    # No build needed - just install the plugin files
    dontBuild = true;

    installPhase = ''
      runHook preInstall
      mkdir -p $out
      # The zip extracts to performancecopilot-pcp-app/
      cp -r performancecopilot-pcp-app/* $out/
      runHook postInstall
    '';

    meta = {
      description = "Performance Co-Pilot Grafana Plugin";
      homepage = "https://github.com/performancecopilot/grafana-pcp";
      license = lib.licenses.asl20;
    };
  };

  # ─── Node Exporter Full dashboard ─────────────────────────────────────
  # Popular Prometheus dashboard for system metrics.
  # Pinned to specific commit for reproducibility.
  # Source: https://github.com/rfmoz/grafana-dashboards
  # Grafana Dashboard ID: 1860
  nodeExporterDashboard = pkgs.fetchurl {
    url = "https://raw.githubusercontent.com/rfmoz/grafana-dashboards/741b1b3878d920439e413c7a7a3ff9cfa8ab2a20/prometheus/node-exporter-full.json";
    sha256 = "1x6r6vrif259zjjzh8m1cdhxr7hnr57ija76vgipyaryh8pyrv33";
  };

  # Create a directory structure for Prometheus dashboards
  prometheusDashboards = pkgs.linkFarm "prometheus-dashboards" [
    { name = "node-exporter-full.json"; path = nodeExporterDashboard; }
  ];

  # ─── Custom PCP dashboards ────────────────────────────────────────────
  # BPF overview dashboard for pmdabpf metrics (bpf.* namespace)
  # The plugin's BCC dashboard uses bcc.* metrics which require pmdabcc
  customDashboards = pkgs.linkFarm "custom-pcp-dashboards" [
    { name = "pcp-bpf-overview.json"; path = ./dashboards/pcp-bpf-overview.json; }
  ];

in
{
  # ═══════════════════════════════════════════════════════════════════════
  # Options interface
  # ═══════════════════════════════════════════════════════════════════════

  options.services.pcp.grafana = {
    enable = mkEnableOption "Grafana with PCP dashboards";

    port = mkOption {
      type = types.port;
      default = constants.ports.grafana;
      description = "Port for Grafana web interface.";
    };

    adminPassword = mkOption {
      type = types.str;
      default = "pcp";
      description = ''
        Grafana admin password.
        Default is 'pcp' for local development - INSECURE.
        Only use for local testing.
      '';
    };

    openFirewall = mkOption {
      type = types.bool;
      default = true;
      description = "Open firewall port for Grafana.";
    };
  };

  # ═══════════════════════════════════════════════════════════════════════
  # Implementation
  # ═══════════════════════════════════════════════════════════════════════

  config = mkIf cfg.enable {
    # Require PCP to be enabled (need pmproxy running for the datasource)
    assertions = [{
      assertion = pcpCfg.enable;
      message = "services.pcp.grafana requires services.pcp.enable = true";
    } {
      assertion = pcpCfg.pmproxy.enable;
      message = "services.pcp.grafana requires services.pcp.pmproxy.enable = true (for PCP Vector datasource)";
    }];

    warnings = [
      "Grafana is configured with insecure default password 'pcp'. Only use for local development."
    ];

    services.grafana = {
      enable = true;

      settings = {
        server = {
          http_addr = "0.0.0.0";  # Allow host access via port forward
          http_port = cfg.port;
        };

        security = {
          admin_user = "admin";
          admin_password = cfg.adminPassword;
        };

        # Disable analytics/phone-home
        analytics.reporting_enabled = false;

        # Allow unsigned community plugin
        plugins.allow_loading_unsigned_plugins = "performancecopilot-pcp-app";

        # Login hint for insecure dev mode
        "auth.basic".enabled = true;
      };

      # ─── Plugin loading ───────────────────────────────────────────────
      # declarativePlugins expects a list of plugin derivations
      declarativePlugins = [ pcpPlugin ];

      # ─── Provisioning ─────────────────────────────────────────────────
      provision = {
        enable = true;

        # Datasources
        datasources.settings.datasources = [
          # PCP Vector - real-time metrics via pmproxy
          {
            name = "PCP Vector";
            type = "performancecopilot-vector-datasource";
            access = "proxy";
            url = "http://localhost:${toString constants.ports.pmproxy}";
            isDefault = true;
            editable = false;
            jsonData = {
              hostspec = "localhost";
            };
          }
        ] ++ optionals (config.services.prometheus.enable or false) [
          # Prometheus - when enabled for comparison
          {
            name = "Prometheus";
            type = "prometheus";
            access = "proxy";
            url = "http://localhost:${toString constants.ports.prometheus}";
            editable = false;
          }
        ];

        # Dashboard providers
        dashboards.settings.providers = [
          # PCP Vector dashboards from plugin
          {
            name = "PCP Vector";
            type = "file";
            folder = "PCP";
            options.path = "${pcpPlugin}/datasources/vector/dashboards";
            disableDeletion = true;
          }
          # Custom PCP dashboards (BPF overview for pmdabpf metrics)
          {
            name = "PCP Custom";
            type = "file";
            folder = "PCP";
            options.path = customDashboards;
            disableDeletion = true;
          }
        ] ++ optionals (config.services.prometheus.enable or false) [
          # Node Exporter Full dashboard for Prometheus comparison
          {
            name = "Prometheus";
            type = "file";
            folder = "Prometheus";
            options.path = prometheusDashboards;
            disableDeletion = true;
          }
        ];
      };
    };

    # ─── Firewall ─────────────────────────────────────────────────────────
    networking.firewall.allowedTCPPorts = mkIf cfg.openFirewall [ cfg.port ];

    # ─── Security hardening ───────────────────────────────────────────────
    # Additional hardening beyond NixOS defaults
    systemd.services.grafana.serviceConfig = {
      # DynamicUser provides additional isolation
      # Note: NixOS grafana module may already set some of these
      ProtectKernelTunables = true;
      ProtectControlGroups = true;
      RestrictNamespaces = true;
      RestrictRealtime = true;
      LockPersonality = true;
    };
  };
}
