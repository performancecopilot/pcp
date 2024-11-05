## Build
FROM quay.io/fedora/fedora:latest AS build
COPY . /usr/src/pcp

WORKDIR /usr/src/pcp
RUN dnf install -y git which rpm-build 'dnf-command(builddep)'
RUN dnf builddep -y build/rpm/redhat.spec
RUN dnf remove -y selinux-policy selinux-policy-targeted

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

RUN dnf install -y --setopt=tsflags=nodocs procps-ng gettext /build/*.rpm redis grafana grafana-pcp && \
    dnf clean all && \
    rm -r /build

COPY build/containers/archive-analysis/root /

RUN systemctl mask systemd-remount-fs.service dev-hugepages.mount sys-fs-fuse-connections.mount systemd-logind.service getty.target console-getty.service systemd-udev-trigger.service systemd-udevd.service systemd-random-seed.service systemd-machine-id-commit.service
RUN systemctl disable pmcd pmlogger pmie && \
    systemctl enable pmproxy redis grafana-server archive-import && \
    mkdir /dashboards

VOLUME ["/archives"]
EXPOSE 3000

CMD ["/usr/sbin/init"]
STOPSIGNAL SIGRTMIN+3
