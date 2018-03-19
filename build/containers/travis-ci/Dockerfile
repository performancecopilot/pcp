FROM fedora:27

RUN dnf -y install which sudo hostname findutils bc git \
                   pkg-config make gcc gcc-c++ perl flex bison \
                   rpm-build redhat-rpm-config \
                   initscripts man ncurses-devel procps readline-devel zlib-devel

COPY . /pcp
CMD ["/usr/sbin/init"]
