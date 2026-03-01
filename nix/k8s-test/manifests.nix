# nix/k8s-test/manifests.nix
#
# Generates Kubernetes manifests for PCP DaemonSet deployment.
# Creates privileged DaemonSet with full node monitoring capabilities.
#
{ pkgs, lib }:
let
  constants = import ./constants.nix { };
  mainConstants = import ../constants.nix;

  # ─── DaemonSet Manifest ────────────────────────────────────────────────
  # Privileged DaemonSet for full node monitoring including BPF metrics
  daemonSetManifest = ''
    apiVersion: v1
    kind: Namespace
    metadata:
      name: ${constants.k8s.namespace}
    ---
    apiVersion: apps/v1
    kind: DaemonSet
    metadata:
      name: ${constants.k8s.daemonSetName}
      namespace: ${constants.k8s.namespace}
      labels:
        app: ${constants.k8s.daemonSetName}
    spec:
      selector:
        matchLabels:
          app: ${constants.k8s.daemonSetName}
      template:
        metadata:
          labels:
            app: ${constants.k8s.daemonSetName}
        spec:
          # Required for seeing all node processes
          hostPID: true

          containers:
          - name: pcp
            image: ${constants.k8s.imageName}:${constants.k8s.imageTag}
            imagePullPolicy: Never

            # Privileged for BPF and full /proc access
            # Run as root to override container's default pcp user
            securityContext:
              privileged: true
              runAsUser: 0

            ports:
            - containerPort: ${toString mainConstants.ports.pmcd}
              name: pmcd
            - containerPort: ${toString mainConstants.ports.pmproxy}
              name: pmproxy

            env:
            # Tell PCP where host filesystem is mounted
            - name: HOST_MOUNT
              value: "/host"
            - name: PCP_SYSFS_DIR
              value: "/host/sys"

            volumeMounts:
            # Host root filesystem (read-only)
            - name: host-root
              mountPath: /host
              readOnly: true
            # Required for BPF
            - name: sys-kernel-debug
              mountPath: /sys/kernel/debug
            # Host /proc for process metrics
            - name: host-proc
              mountPath: /host/proc
              readOnly: true
            # Host /sys for system metrics
            - name: host-sys
              mountPath: /host/sys
              readOnly: true

            readinessProbe:
              tcpSocket:
                port: pmcd
              initialDelaySeconds: 10
              periodSeconds: 5

            resources:
              limits:
                memory: "512Mi"
                cpu: "500m"
              requests:
                memory: "256Mi"
                cpu: "100m"

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
  # The full DaemonSet manifest as a string
  manifest = daemonSetManifest;

  # Write manifest to a file for kubectl apply
  manifestFile = pkgs.writeText "pcp-daemonset.yaml" daemonSetManifest;

  # Helper to get the manifest path
  getManifestPath = "${pkgs.writeText "pcp-daemonset.yaml" daemonSetManifest}";
}
