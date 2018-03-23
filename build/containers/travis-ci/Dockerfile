FROM fedora:27

RUN dnf -y install which sudo hostname findutils bc git \
                   pkg-config make gcc gcc-c++ perl flex bison \
                   rpm-build redhat-rpm-config \
                   initscripts man ncurses-devel procps readline-devel zlib-devel \
                   python3 python3-devel bcc-tools

COPY . /pcp
RUN cd /pcp && ./Makepkgs
RUN cd /pcp/pcp-*/build/rpm && VER=$(pwd | cut -d- -f2 | cut -d/ -f1) && \
    dnf -y install pcp{,-conf,-libs,-libs-devel,-devel,-testsuite,-pmda-bcc}-$VER-1.x86_64.rpm \
    python3-pcp-$VER-1.x86_64.rpm

CMD ["/usr/sbin/init"]
