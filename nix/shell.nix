# nix/shell.nix
#
# Development shell for PCP.
# Provides build dependencies plus debugging tools.
#
{ pkgs, pcp }:
let
  lib = pkgs.lib;
in
pkgs.mkShell {
  inputsFrom = [ pcp ];
  packages = with pkgs; [
    gdb
    jp2a
  ] ++ lib.optionals pkgs.stdenv.isLinux [
    valgrind
    # K8s testing tools
    minikube
    kubectl
    docker
  ] ++ lib.optionals pkgs.stdenv.isDarwin [
    lldb
  ];
  shellHook = ''
    if [[ -f ./images/pcpicon-light.png ]]; then
      jp2a --colors ./images/pcpicon-light.png 2>/dev/null || true
    fi
    echo "PCP Development Shell"
    echo "Run './configure --help' to see build options"
    echo "Otherwise use 'nix build' to build the package"
  '';
}
