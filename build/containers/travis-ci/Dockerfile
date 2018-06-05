FROM fedora:27

RUN dnf -y install \
        which sudo hostname findutils bc git \
        pkg-config make gcc gcc-c++ perl flex bison \
        rpm-build redhat-rpm-config initscripts man procps \
        avahi-devel ncurses-devel readline-devel zlib-devel \
        perl perl-devel perl-generators perl-ExtUtils-MakeMaker \
        python3 python3-devel \
        bcc-tools e2fsprogs xfsprogs

COPY . /pcp
RUN cd /pcp && ./Makepkgs
RUN cd /pcp/pcp-*/build/rpm && VER=$(pwd | cut -d- -f2 | cut -d/ -f1) && \
    dnf -y install \
        pcp-$VER-1.x86_64.rpm pcp-{conf,libs}-$VER-1.x86_64.rpm \
        pcp-{libs-devel,devel,testsuite}-$VER-1.x86_64.rpm \
        pcp-pmda-{bcc,cisco,news}-$VER-1.x86_64.rpm \
        perl-PCP-{LogImport,LogSummary,MMV,PMDA}-$VER-1.x86_64.rpm \
        python3-pcp-$VER-1.x86_64.rpm

CMD ["/usr/sbin/init"]
