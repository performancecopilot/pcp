# rpm build container
FROM registry.fedoraproject.org/fedora:34
COPY . /usr/src/pcp

WORKDIR /usr/src/pcp
RUN dnf install -y git rpm-build 'dnf-command(builddep)' && \
    # pmdabind2
    dnf install -y perl-XML-LibXML perl-File-Slurp && \
    dnf builddep -y build/rpm/redhat.spec && \
    mkdir -p /root/rpmbuild/SOURCES && \
    ./Makepkgs --source && \
    mv build/tar/pcp-*.src.tar.gz /root/rpmbuild/SOURCES && \
    rpmbuild -bb build/rpm/redhat.spec

WORKDIR /root/rpmbuild/RPMS/x86_64
RUN mkdir /build && \
    release=$(ls pcp-zeroconf-* | sed -E 's/pcp-zeroconf-(.+)\.(.+)\.rpm/\1/') && \
    cp /root/rpmbuild/RPMS/noarch/pcp-doc-$release.noarch.rpm /build && \
    cp \
      pcp-$release.x86_64.rpm \
      pcp-zeroconf-$release.x86_64.rpm \
      pcp-conf-$release.x86_64.rpm \
      pcp-libs-$release.x86_64.rpm \
      pcp-selinux-$release.x86_64.rpm \
      pcp-system-tools-$release.x86_64.rpm \
      pcp-pmda-dm-$release.x86_64.rpm \
      pcp-pmda-nfsclient-$release.x86_64.rpm \
      pcp-pmda-openmetrics-$release.x86_64.rpm \
      python3-pcp-$release.x86_64.rpm \
      /build

# actual container
FROM registry.fedoraproject.org/fedora:34
COPY --from=0 /build/ /build/

ENV SUMMARY="Performance Co-Pilot" \
    DESCRIPTION="Performance Co-Pilot is a system performance analysis toolkit." \
    VERSION=5

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
    dnf clean all

COPY build/container/root /

VOLUME ["/var/log/pcp/pmlogger"]
EXPOSE 44321
EXPOSE 44322

ENTRYPOINT ["/usr/bin/container-entrypoint"]
CMD ["/usr/sbin/init"]
