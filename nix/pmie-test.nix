# nix/pmie-test.nix
#
# pmie testing module with synthetic workload.
# Creates predictable CPU spikes that a dedicated pmie instance detects.
#
# This module provides:
# 1. stress-ng-test service: Periodic CPU load (20s on, 10s off)
# 2. pmie-test service: Dedicated pmie instance monitoring for the workload
# 3. Alert logging to /var/log/pcp/pmie/alerts.log
#
# Usage:
#   imports = [ ./pmie-test.nix ];
#   services.pcp.pmieTest.enable = true;
#
{ config, pkgs, lib, ... }:

with lib;
let
  cfg = config.services.pcp.pmieTest;
  pcpCfg = config.services.pcp;

  # Stress workload script - creates periodic CPU load for pmie to detect
  stressScript = pkgs.writeShellApplication {
    name = "stress-ng-test";
    runtimeInputs = [ pkgs.stress-ng pkgs.coreutils pkgs.util-linux ];
    text = ''
      STRESS_DURATION=''${STRESS_DURATION:-20}
      IDLE_DURATION=''${IDLE_DURATION:-10}
      CPU_WORKERS=''${CPU_WORKERS:-2}

      echo "stress-ng-test: ''${STRESS_DURATION}s stress, ''${IDLE_DURATION}s idle, ''${CPU_WORKERS} workers"

      while true; do
        logger -t stress-ng-test "Starting stress cycle"
        stress-ng --cpu "$CPU_WORKERS" --timeout "''${STRESS_DURATION}s" --quiet || true
        logger -t stress-ng-test "Idle period"
        sleep "$IDLE_DURATION"
      done
    '';
  };

  # Helper scripts for pmie actions (pmie shell action needs simple commands)
  # Use absolute paths since pmie doesn't have PATH set
  alertScript = pkgs.writeShellScript "pmie-alert" ''
    ${pkgs.coreutils}/bin/echo "$(${pkgs.coreutils}/bin/date -Iseconds) [ALERT] CPU elevated" >> /var/log/pcp/pmie/alerts.log
  '';

  heartbeatScript = pkgs.writeShellScript "pmie-heartbeat" ''
    ${pkgs.coreutils}/bin/touch /var/log/pcp/pmie/heartbeat
  '';

  # pmie rules for detecting the stress-ng workload
  # Note: pmie shell actions need simple command paths, not complex shell invocations
  pmieRules = pkgs.writeText "pmie-test.rules" ''
    //
    // pmie test rules - detect stress-ng-test workload
    // Deployed by nix/pmie-test.nix
    //

    delta = 5 sec;

    // Rule 1: Detect elevated CPU (>10% nice time) and log
    // Note: stress-ng runs at Nice=19 with CPUQuota=50%, so CPU nice is ~0.5
    // With 4 CPUs, threshold of 0.10*4=0.4 should trigger during stress
    cpu_elevated =
        kernel.all.cpu.nice > 0.10 * hinv.ncpu
        -> shell "${alertScript}";

    // Rule 2: Heartbeat to confirm pmie is evaluating
    heartbeat =
        hinv.ncpu >= 1
        -> shell "${heartbeatScript}";
  '';

  # PCP environment variables (same as main module)
  pcpConf = "${pcpCfg.package}/share/pcp/etc/pcp.conf";
  pcpDir = "${pcpCfg.package}/share/pcp";
  pcpEnv = {
    PCP_CONF = pcpConf;
    PCP_DIR = pcpDir;
    PCP_LOG_DIR = "/var/log/pcp";
    PCP_VAR_DIR = "/var/lib/pcp";
    PCP_TMP_DIR = "/var/lib/pcp/tmp";
    PCP_RUN_DIR = "/run/pcp";
  };

in
{
  options.services.pcp.pmieTest = {
    enable = mkEnableOption "pmie testing with stress-ng workload";

    stressDuration = mkOption {
      type = types.int;
      default = 20;
      description = "Seconds to run stress-ng.";
    };

    idleDuration = mkOption {
      type = types.int;
      default = 10;
      description = "Seconds to idle between stress cycles.";
    };

    cpuWorkers = mkOption {
      type = types.int;
      default = 2;
      description = "Number of CPU stress workers.";
    };
  };

  config = mkIf cfg.enable {
    # Require PCP to be enabled (need pmcd running)
    assertions = [{
      assertion = pcpCfg.enable;
      message = "services.pcp.pmieTest requires services.pcp.enable = true";
    }];

    # Install stress-ng
    environment.systemPackages = [ pkgs.stress-ng ];

    # Create the stress workload service
    systemd.services.stress-ng-test = {
      description = "Synthetic CPU workload for pmie testing";
      wantedBy = [ "multi-user.target" ];
      after = [ "pmcd.service" ];
      wants = [ "pmcd.service" ];

      environment = {
        STRESS_DURATION = toString cfg.stressDuration;
        IDLE_DURATION = toString cfg.idleDuration;
        CPU_WORKERS = toString cfg.cpuWorkers;
      };

      serviceConfig = {
        Type = "simple";
        ExecStart = "${stressScript}/bin/stress-ng-test";
        Restart = "always";
        RestartSec = "5s";

        # Low priority - won't interfere with other services
        Nice = 19;
        IOSchedulingClass = "idle";

        # Limit resource usage
        CPUQuota = "50%";
        MemoryMax = "128M";

        # Security
        NoNewPrivileges = true;
        ProtectHome = true;
        PrivateTmp = true;
      };
    };

    # Dedicated pmie service for testing (runs our custom rules)
    # This is separate from the main pmie service to avoid conflicts
    systemd.services.pmie-test = {
      description = "Performance Co-Pilot Inference Engine (Test Rules)";
      wantedBy = [ "multi-user.target" ];
      after = [ "pmcd.service" "stress-ng-test.service" ];
      bindsTo = [ "pmcd.service" ];

      environment = pcpEnv;

      serviceConfig = {
        Type = "simple";
        # Run pmie directly with our rules file
        # -f = run in foreground (for systemd)
        # -c = config file
        # -l = log file
        ExecStart = "${pcpCfg.package}/bin/pmie -f -c ${pmieRules} -l /var/log/pcp/pmie/pmie-test.log";
        Restart = "on-failure";
        RestartSec = "5s";
        User = "pcp";
        Group = "pcp";
      };
    };

    # Add pmie directories and alert log file
    # Note: nixos-module.nix also creates /var/log/pcp/pmie via tmpfiles
    systemd.tmpfiles.rules = [
      "d /var/log/pcp/pmie 0755 pcp pcp -"
      "f /var/log/pcp/pmie/alerts.log 0644 pcp pcp -"
    ];
  };
}
