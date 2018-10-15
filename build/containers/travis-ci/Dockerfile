FROM fedora:28

RUN dnf -y install \
        which sudo hostname findutils bc git \
        pkg-config make gcc gcc-c++ perl flex bison \
        rpm-build redhat-rpm-config initscripts man procps \
        avahi-devel ncurses-devel readline-devel zlib-devel \
        perl perl-devel perl-generators perl-ExtUtils-MakeMaker \
        python2-devel python3 python3-devel \
        bcc-tools e2fsprogs xfsprogs

COPY . /pcp
RUN cd /pcp && ./Makepkgs --verbose
RUN cd /pcp/pcp-*/build/rpm && VER=$(pwd | cut -d- -f2 | cut -d/ -f1) && \
    dnf -y install  *-$VER-*.{x86_64,noarch}.rpm

CMD ["/usr/sbin/init"]
