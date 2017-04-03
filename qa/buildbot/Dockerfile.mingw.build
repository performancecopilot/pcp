FROM fedora:24
MAINTAINER PCP Development Team <pcp@groups.io>
ENV container docker
STOPSIGNAL SIGRTMIN+3
RUN dnf upgrade -y && dnf install -y time which bc sudo git make gcc gcc-c++ zsh rpm-build dnf-plugins-core avahi-tools avahi-glib avahi-autoipd valgrind mingw64-* && dnf builddep -y pcp && dnf clean all

RUN mkdir -p /pcpsrc/ && cd /pcpsrc/ && git clone git://git.pcp.io/pcp/pcp.git && cd pcp && ./Makepkgs --verbose
RUN export PCPMAJ=$(grep "MAJOR" /pcpsrc/pcp/VERSION.pcp | cut -f2 -d=); export PCPMIN=$(grep "MINOR" /pcpsrc/pcp/VERSION.pcp | cut -f2 -d=); export PCPREV=$(grep "REVISION" /pcpsrc/pcp/VERSION.pcp | cut -f2 -d=); cd /pcpsrc/pcp/pcp-$PCPMAJ.$PCPMIN.$PCPREV/build/rpm && rm ./*src.rpm && dnf install ./*rpm -y
CMD cd /pcpsrc/pcp/ && ./Makepkgs --target mingw64 --verbose
