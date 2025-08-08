## Build
FROM quay.io/fedora/fedora:latest AS build
COPY . /usr/src/pcp

WORKDIR /usr/src/pcp
RUN dnf install -y git which bpftrace rpm-build 'dnf-command(builddep)'
RUN dnf builddep -y build/rpm/redhat.spec

RUN ./Makepkgs --verbose
RUN mkdir /build && \
    cd pcp-*/build/rpm && \
    machine=$(ls pcp-zeroconf-* | sed -E 's/pcp-zeroconf-(.+)\.(.+)\.rpm/\2/') && \
    release=$(ls pcp-zeroconf-* | sed -E 's/pcp-zeroconf-(.+)\.(.+)\.rpm/\1/') && \
    cp \
      pcp-$release.$machine.rpm \
      pcp-conf-$release.$machine.rpm \
      pcp-doc-$release.noarch.rpm \
      pcp-libs-$release.$machine.rpm \
      pcp-system-tools-$release.$machine.rpm \
      pcp-zeroconf-$release.$machine.rpm \
      pcp-pmda-bpf-$release.$machine.rpm \
      pcp-pmda-bpftrace-$release.$machine.rpm \
      pcp-pmda-dm-$release.$machine.rpm \
      pcp-pmda-nfsclient-$release.$machine.rpm \
      pcp-pmda-openmetrics-$release.$machine.rpm \
      pcp-pmda-opentelemetry-$release.$machine.rpm \
      python3-pcp-$release.$machine.rpm \
      /build

## Deploy
FROM quay.io/fedora/fedora:latest
COPY --from=build /build /build

ENV SUMMARY="Performance Co-Pilot"
ENV DESCRIPTION="Performance Co-Pilot is a system performance analysis toolkit."
ENV VERSION=7

LABEL name="pcp"
LABEL summary="${SUMMARY}"
LABEL description="${DESCRIPTION}"
LABEL version="$VERSION"
LABEL usage="podman run -d --name pcp --systemd always -p 44321:44321 -p 44322:44322 -v pmlogger:/var/log/pcp/pmlogger -v pmproxy:/var/log/pcp/pmproxy quay.io/performancecopilot/pcp"
LABEL maintainer="PCP Team <pcp@groups.io>"
LABEL help="cat /README.md"
LABEL com.redhat.component="pcp"
LABEL io.k8s.display-name="Performance Co-Pilot"
LABEL io.k8s.description="${DESCRIPTION}"
LABEL io.openshift.expose-services="44321:pmcd,44322:pmproxy"
LABEL io.openshift.tags="pcp,performance,metrics,analysis,monitoring,observability"

RUN dnf install -y --setopt=tsflags=nodocs procps-ng gettext /build/*.rpm
RUN dnf clean all
RUN rm -r /build

COPY build/containers/pcp/root /

RUN systemctl mask systemd-remount-fs.service dev-hugepages.mount sys-fs-fuse-connections.mount systemd-logind.service getty.target console-getty.service systemd-udev-trigger.service systemd-udevd.service systemd-random-seed.service systemd-machine-id-commit.service

VOLUME ["/var/log/pcp/pmlogger"]
VOLUME ["/var/log/pcp/pmproxy"]
EXPOSE 44321
EXPOSE 44322

ENTRYPOINT ["/usr/bin/container-entrypoint"]
CMD ["/usr/sbin/init"]
STOPSIGNAL SIGRTMIN+3
