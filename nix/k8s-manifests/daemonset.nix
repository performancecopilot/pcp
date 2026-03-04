# nix/k8s-manifests/daemonset.nix
#
# Generates the Kubernetes DaemonSet resource YAML for PCP deployment.
# Creates a privileged DaemonSet with full node monitoring capabilities
# including BPF metrics.
#
{ pkgs, constants }:
let
  k = constants.k8s;

  yaml = ''
    apiVersion: apps/v1
    kind: DaemonSet
    metadata:
      name: ${k.daemonSetName}
      namespace: ${k.namespace}
      labels:
        app: ${k.daemonSetName}
    spec:
      selector:
        matchLabels:
          app: ${k.daemonSetName}
      template:
        metadata:
          labels:
            app: ${k.daemonSetName}
        spec:
          # Required for seeing all node processes
          hostPID: true

          containers:
          - name: pcp
            image: ${k.image.name}:${k.image.tag}
            imagePullPolicy: ${k.image.pullPolicy}

            # Privileged for BPF and full /proc access
            # Run as root to override container's default pcp user
            securityContext:
              privileged: true
              runAsUser: 0

            ports:
            - containerPort: ${toString constants.ports.pmcd}
              name: pmcd
            - containerPort: ${toString constants.ports.pmproxy}
              name: pmproxy

            env:
            # Tell PCP where host filesystem is mounted
            - name: HOST_MOUNT
              value: "${k.hostMounts.root}"
            - name: PCP_SYSFS_DIR
              value: "${k.hostMounts.sys}"

            volumeMounts:
            # Host root filesystem (read-only)
            - name: host-root
              mountPath: ${k.hostMounts.root}
              readOnly: true
            # Required for BPF
            - name: sys-kernel-debug
              mountPath: ${k.hostMounts.kernelDebug}
            # Host /proc for process metrics
            - name: host-proc
              mountPath: ${k.hostMounts.proc}
              readOnly: true
            # Host /sys for system metrics
            - name: host-sys
              mountPath: ${k.hostMounts.sys}
              readOnly: true

            readinessProbe:
              tcpSocket:
                port: pmcd
              initialDelaySeconds: 10
              periodSeconds: 5

            resources:
              limits:
                memory: "${k.resources.limits.memory}"
                cpu: "${k.resources.limits.cpu}"
              requests:
                memory: "${k.resources.requests.memory}"
                cpu: "${k.resources.requests.cpu}"

          volumes:
          - name: host-root
            hostPath:
              path: /
          - name: sys-kernel-debug
            hostPath:
              path: /sys/kernel/debug
          - name: host-proc
            hostPath:
              path: /proc
          - name: host-sys
            hostPath:
              path: /sys

          # Run on all nodes including control plane
          tolerations:
          - operator: Exists
  '';
in
{
  inherit yaml;
  file = pkgs.writeText "pcp-daemonset.yaml" yaml;
}
