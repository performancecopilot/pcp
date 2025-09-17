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

RUN dnf install -y --setopt=tsflags=nodocs gettext-envsubst /build/*.rpm
RUN dnf clean all
RUN rm -r /build

# ensure a fixed UID is assigned for every PCP container invocation
RUN sed -i -e 's/^u pcp -/u pcp 1001/g' /usr/lib/sysusers.d/pcp.conf

## Setup
COPY build/containers/pcp/root /

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

RUN systemctl mask \
    console-getty.service dev-hugepages.mount getty.target \
    sys-fs-fuse-connections.mount \
    systemd-oomd.service systemd-resolved.service \
    systemd-logind.service systemd-machine-id-commit.service \
    systemd-random-seed.service systemd-remount-fs.service \
    systemd-udev-trigger.service systemd-udevd.service \
    systemd-udevd-control.socket systemd-udevd-kernel.socket

VOLUME ["/var/log/pcp/pmlogger"]
VOLUME ["/var/log/pcp/pmproxy"]
EXPOSE 44321
EXPOSE 44322

ENTRYPOINT ["/usr/bin/container-entrypoint"]
CMD ["/usr/sbin/init"]
STOPSIGNAL SIGRTMIN+3
