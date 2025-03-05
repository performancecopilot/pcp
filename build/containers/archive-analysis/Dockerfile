## Build
FROM quay.io/fedora/fedora:latest AS build
COPY . /usr/src/pcp

WORKDIR /usr/src/pcp
RUN dnf install -y git which rpm-build 'dnf-command(builddep)'
RUN dnf builddep -y build/rpm/redhat.spec
RUN dnf remove -y selinux-policy

RUN ./Makepkgs --verbose
RUN mkdir /build && \
    cd pcp-*/build/rpm && \
    release=$(ls pcp-zeroconf-* | sed -E 's/pcp-zeroconf-(.+)\.(.+)\.rpm/\1/') && \
    cp \
      pcp-$release.x86_64.rpm \
      pcp-conf-$release.x86_64.rpm \
      pcp-libs-$release.x86_64.rpm \
      /build

## Deploy
FROM quay.io/fedora/fedora:latest
COPY --from=build /build /build

COPY build/containers/archive-analysis/root /

RUN dnf install -y --setopt=install_weak_deps=False --setopt=tsflags=nodocs \
        python3 procps-ng gettext /build/*.rpm valkey grafana grafana-pcp && \
    dnf clean all && \
    rm -rf /build && \
    systemctl mask systemd-remount-fs.service dev-hugepages.mount \
        sys-fs-fuse-connections.mount systemd-logind.service getty.target \
        console-getty.service systemd-udev-trigger.service systemd-udevd.service \
        systemd-random-seed.service systemd-machine-id-commit.service \
        systemd-oomd systemd-resolved \
        systemd-udevd-control.socket systemd-udevd-kernel.socket && \
    systemctl disable pmcd pmlogger pmie && \
    systemctl enable pmproxy valkey grafana-server archive-import && \
    mkdir /dashboards && \
    sed -i 's/pcp-valkey-datasource/performancecopilot-valkey-datasource/g' \
        /etc/grafana/grafana.ini && \
    chown -R grafana:grafana /etc/grafana

VOLUME ["/archives"]
EXPOSE 3000

CMD ["/usr/sbin/init"]
STOPSIGNAL SIGRTMIN+3
