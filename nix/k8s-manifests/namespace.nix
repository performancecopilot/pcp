# nix/k8s-manifests/namespace.nix
#
# Generates the Kubernetes Namespace resource YAML for PCP deployment.
#
{ pkgs, constants }:
let
  yaml = ''
    apiVersion: v1
    kind: Namespace
    metadata:
      name: ${constants.k8s.namespace}
  '';
in
{
  inherit yaml;
  file = pkgs.writeText "pcp-namespace.yaml" yaml;
}
