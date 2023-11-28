## Build
FROM registry.fedoraproject.org/fedora:38 AS build
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
      pcp-doc-$release.noarch.rpm \
      pcp-libs-$release.x86_64.rpm \
      pcp-system-tools-$release.x86_64.rpm \
      pcp-zeroconf-$release.x86_64.rpm \
      pcp-pmda-dm-$release.x86_64.rpm \
      pcp-pmda-nfsclient-$release.x86_64.rpm \
      pcp-pmda-openmetrics-$release.x86_64.rpm \
      python3-pcp-$release.x86_64.rpm \
      /build

## Deploy
FROM registry.fedoraproject.org/fedora:38
COPY --from=build /build /build

ENV SUMMARY="Performance Co-Pilot" \
    DESCRIPTION="Performance Co-Pilot is a system performance analysis toolkit." \
    VERSION=6

LABEL name="pcp" \
      summary="${SUMMARY}" \
      description="${DESCRIPTION}" \
      version="$VERSION" \
      usage="podman run -d --name pcp --systemd always -p 44321:44321 -p 44322:44322 -v pcp-archives:/var/log/pcp/pmlogger quay.io/performancecopilot/pcp" \
      maintainer="PCP Team <pcp@groups.io>" \
      help="cat /README.md" \
      com.redhat.component="pcp" \
      io.k8s.display-name="Performance Co-Pilot" \
      io.k8s.description="${DESCRIPTION}" \
      io.openshift.expose-services="44321:pmcd,44322:pmproxy" \
      io.openshift.tags="pcp,performance,monitoring,metrics"

RUN groupadd -g 1001 -r pcp && \
    useradd  -u 1001 -g pcp -r -d /var/lib/pcp -s /sbin/nologin -c "Performance Co-Pilot" pcp && \
    dnf install -y --setopt=tsflags=nodocs procps-ng gettext /build/*.rpm && \
    dnf clean all && \
    rm -r /build

COPY build/containers/pcp/root /

RUN systemctl mask systemd-remount-fs.service dev-hugepages.mount sys-fs-fuse-connections.mount systemd-logind.service getty.target console-getty.service systemd-udev-trigger.service systemd-udevd.service systemd-random-seed.service systemd-machine-id-commit.service

VOLUME ["/var/log/pcp/pmlogger"]
EXPOSE 44321
EXPOSE 44322

ENTRYPOINT ["/usr/bin/container-entrypoint"]
CMD ["/usr/sbin/init"]
STOPSIGNAL SIGRTMIN+3
