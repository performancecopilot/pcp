#
# flake.nix - PCP Nix packaging
#
# See also: ./docs/HowTos/nix/index.rst
{
  description = "Performance Co-Pilot (PCP) - system performance monitoring toolkit";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs =
    {
      self,
      nixpkgs,
      flake-utils,
    }:
    flake-utils.lib.eachDefaultSystem (
      system:
      let
        pkgs = nixpkgs.legacyPackages.${system};
        lib = pkgs.lib;

        pcp = pkgs.stdenv.mkDerivation rec {
          pname = "pcp";
          version = "7.0.5";

          src = ./.;

          outputs = [
            "out"
            "man"
            "doc"
          ];

          nativeBuildInputs = with pkgs; [
            autoconf
            automake
            pkg-config
            bison
            flex
            which
            perl
            python3
            python3.pkgs.setuptools
          ] ++ lib.optionals withBpf [
            llvmPackages.clang
            llvmPackages.llvm
          ];

          buildInputs = with pkgs; [
            zlib
            ncurses
            readline
            openssl
            libuv
            cyrus_sasl
            inih
            xz
            python3
            perl
            rrdtool
          ] ++ lib.optionals pkgs.stdenv.isLinux [
            avahi
            lvm2
          ] ++ lib.optionals withSystemd [
            systemd
          ] ++ lib.optionals withPfm [
            libpfm
          ] ++ lib.optionals withBpf [
            libbpf
            bcc
            elfutils
          ] ++ lib.optionals withSnmp [
            net-snmp
          ] ++ lib.optionals withPythonHttp [
            python3.pkgs.requests
          ] ++ lib.optionals withPerlHttp [
            perlPackages.JSON
            perlPackages.LWPUserAgent
          ];

          withSystemd = pkgs.stdenv.isLinux;
          withPfm = pkgs.stdenv.isLinux;
          withBpf = pkgs.stdenv.isLinux;
          withSnmp = true;
          withPythonHttp = true;
          withPerlHttp = true;

          configureFlags = lib.concatLists [

            [
              "--prefix=${placeholder "out"}"
              "--sysconfdir=${placeholder "out"}/etc"
              "--localstatedir=${placeholder "out"}/var"
              "--with-rcdir=${placeholder "out"}/etc/init.d"
              "--with-tmpdir=/tmp"
              "--with-logdir=${placeholder "out"}/var/log/pcp"
              "--with-rundir=/run/pcp"
            ]

            [
              "--with-user=pcp"
              "--with-group=pcp"
            ]

            [
              "--with-make=make"
              "--with-tar=tar"
              "--with-python3=${lib.getExe pkgs.python3}"
            ]

            [
              "--with-perl=yes"
              "--with-threads=yes"
            ]

            [
              "--with-secure-sockets=yes"
              "--with-transparent-decompression=yes"
            ]

            (if pkgs.stdenv.isLinux then [ "--with-discovery=yes" ] else [ "--with-discovery=no" ])

            [
              "--with-dstat-symlink=no"
              "--with-pmdamongodb=no"
              "--with-pmdamysql=no"
              "--with-pmdanutcracker=no"
              "--with-qt=no"
              "--with-infiniband=no"
              "--with-selinux=no"
            ]

            (if withSystemd then [ "--with-systemd=yes" ] else [ "--with-systemd=no" ])
            (if withPfm then [ "--with-perfevent=yes" ] else [ "--with-perfevent=no" ])
            (
              if withBpf then
                [
                  "--with-pmdabcc=yes"
                  "--with-pmdabpf=yes"
                  "--with-pmdabpftrace=yes"
                ]
              else
                [
                  "--with-pmdabcc=no"
                  "--with-pmdabpf=no"
                  "--with-pmdabpftrace=no"
                ]
            )

            (if pkgs.stdenv.isLinux then [ "--with-devmapper=yes" ] else [ "--with-devmapper=no" ])

            (if withSnmp then [ "--with-pmdasnmp=yes" ] else [ "--with-pmdasnmp=no" ])
          ];


          patches = [
            ./nix/patches/gnumakefile-nix-fixes.patch
            ./nix/patches/tmpdir-portability.patch
          ];

          postPatch = ''
            # Fix shebangs (can't be done as static patch - needs Nix store paths)
            patchShebangs src build configure scripts man
          '';

          hardeningDisable = lib.optionals withBpf [ "zerocallusedregs" ];

          BPF_CFLAGS = lib.optionalString withBpf "-fno-stack-protector -Wno-error=unused-command-line-argument";
          CLANG = lib.optionalString withBpf (lib.getExe pkgs.llvmPackages.clang);

          SYSTEMD_SYSTEMUNITDIR = lib.optionalString withSystemd "${placeholder "out"}/lib/systemd/system";
          SYSTEMD_TMPFILESDIR = lib.optionalString withSystemd "${placeholder "out"}/lib/tmpfiles.d";
          SYSTEMD_SYSUSERSDIR = lib.optionalString withSystemd "${placeholder "out"}/lib/sysusers.d";

          preConfigure = ''
            export AR="${pkgs.stdenv.cc.bintools}/bin/ar"
          '';

          postInstall = ''
            # Build the combined PMNS root file
            # The individual root_* files exist but pmcd needs a combined 'root' file
            # Use pmnsmerge to combine all the root_* files into one
            (
              cd $out/var/lib/pcp/pmns
              export PCP_DIR=$out
              export PCP_CONF=$out/etc/pcp.conf
              . $out/etc/pcp.env

              # Merge all the root_* files into the combined root file
              # Order matters: root_root first (base), then others
              $out/libexec/pcp/bin/pmnsmerge -a \
                $out/libexec/pcp/pmns/root_root \
                $out/libexec/pcp/pmns/root_pmcd \
                $out/libexec/pcp/pmns/root_linux \
                $out/libexec/pcp/pmns/root_proc \
                $out/libexec/pcp/pmns/root_xfs \
                $out/libexec/pcp/pmns/root_jbd2 \
                $out/libexec/pcp/pmns/root_kvm \
                $out/libexec/pcp/pmns/root_mmv \
                $out/libexec/pcp/pmns/root_bpf \
                $out/libexec/pcp/pmns/root_pmproxy \
                root
            )

            # Remove runtime state directories
            rm -rf $out/var/{run,log} $out/var/lib/pcp/tmp || true

            # Move vendor config to share
            if [ -d "$out/etc" ]; then
              mkdir -p $out/share/pcp/etc
              mv $out/etc/* $out/share/pcp/etc/
              rmdir $out/etc || true

              # Fix paths in pcp.conf to point to new locations
              substituteInPlace $out/share/pcp/etc/pcp.conf \
                --replace-quiet "$out/etc/pcp" "$out/share/pcp/etc/pcp" \
                --replace-quiet "$out/etc/sysconfig" "$out/share/pcp/etc/sysconfig" \
                --replace-quiet "PCP_ETC_DIR=$out/etc" "PCP_ETC_DIR=$out/share/pcp/etc"

              # Fix symlinks that pointed to /etc/pcp/...
              find $out/var/lib/pcp -type l | while read link; do
                target=$(readlink "$link")
                if [[ "$target" == *"/etc/pcp/"* ]]; then
                  suffix="''${target#*/etc/pcp/}"
                  rm "$link"
                  ln -sf "$out/share/pcp/etc/pcp/$suffix" "$link"
                fi
              done
            fi

            # Fix broken symlinks with double /nix/store prefix
            # These occur when the build system prepends a path to an already-absolute path
            for broken_link in "$out/share/pcp/etc/pcp/pm"{search/pmsearch,series/pmseries}.conf; do
              [[ -L "$broken_link" ]] && rm "$broken_link" && \
                ln -sf "$out/share/pcp/etc/pcp/pmproxy/pmproxy.conf" "$broken_link"
            done

            # Fix pmcd/rc.local symlink (points to libexec/pcp/services/local)
            if [[ -L "$out/share/pcp/etc/pcp/pmcd/rc.local" ]]; then
              rm "$out/share/pcp/etc/pcp/pmcd/rc.local"
              ln -sf "$out/libexec/pcp/services/local" "$out/share/pcp/etc/pcp/pmcd/rc.local"
            fi

            # Move man pages to $man output
            if [ -d "$out/share/man" ]; then
              mkdir -p $man/share
              mv $out/share/man $man/share/
            fi

            # Move documentation to $doc output
            for docdir in $out/share/doc/pcp*; do
              if [ -d "$docdir" ]; then
                mkdir -p $doc/share/doc
                mv "$docdir" $doc/share/doc/
              fi
            done
          '';

          doCheck = false;
          enableParallelBuilding = true;

          meta = with lib; {
            description = "Performance Co-Pilot - system performance monitoring toolkit";
            homepage = "https://pcp.io";
            license = licenses.gpl2Plus;
            platforms = platforms.linux ++ platforms.darwin;
            mainProgram = "pminfo";
          };
        };
      in
      {
        packages = {
          default = pcp;
          inherit pcp;
        };

        checks = lib.optionalAttrs pkgs.stdenv.isLinux {
          vm-test = import ./nix/vm-test.nix {
            inherit pkgs pcp;
          };
        };

        devShells.default = pkgs.mkShell {
          inputsFrom = [ pcp ];
          packages = with pkgs; [
            gdb
            jp2a
          ] ++ lib.optionals pkgs.stdenv.isLinux [
            valgrind
          ] ++ lib.optionals pkgs.stdenv.isDarwin [
            lldb
          ];

          shellHook = ''
            # Display PCP logo on shell entry
            if [[ -f ./images/pcpicon-light.png ]]; then
              jp2a --colors ./images/pcpicon-light.png 2>/dev/null || true
            fi
            echo "PCP Development Shell"
            echo "Run './configure --help' to see build options"
            echo "Otherwise use 'nix build' to build the package"
          '';
        };
      }
    );
}

