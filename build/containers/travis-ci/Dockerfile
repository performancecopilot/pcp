FROM fedora:28

RUN dnf -y install \
        pkg-config make gcc flex bison \
        which sudo hostname findutils bc git cppcheck \
        rpm-build redhat-rpm-config initscripts man procps \
        avahi-devel ncurses-devel readline-devel zlib-devel \
        perl perl-devel perl-generators perl-ExtUtils-MakeMaker \
        python2-devel python3 python3-devel pylint \
        bcc-tools e2fsprogs xfsprogs

COPY . /pcp
RUN cd /pcp && ./Makepkgs --check --verbose \
	--without-qt --without-qt3d --without-webapi --without-manager
RUN cd /pcp/pcp-*/build/rpm && VER=$(pwd | cut -d- -f2 | cut -d/ -f1) && \
    dnf -y install  *-$VER-*.{x86_64,noarch}.rpm
RUN sed -ie '/\[Service\]/aTimeoutSec=120' /usr/lib/systemd/system/{pmcd,pmlogger,pmie,pmproxy}.service
CMD ["/usr/sbin/init"]
