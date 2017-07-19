FROM fedora:25
MAINTAINER PCP Development Team <pcp@groups.io>
ENV container docker
STOPSIGNAL SIGRTMIN+3
RUN dnf upgrade -y && dnf install -y time which bc sudo git make gcc gcc-c++ zsh rpm-build dnf-plugins-core avahi-tools avahi-glib avahi-autoipd valgrind && dnf builddep -y pcp && dnf clean all

RUN mkdir -p /pcpsrc/ && cd /pcpsrc/ && git clone git://git.pcp.io/pcp/pcp.git && cd pcp && ./Makepkgs --verbose
RUN export PCPMAJ=$(grep "MAJOR" /pcpsrc/pcp/VERSION.pcp | cut -f2 -d=); export PCPMIN=$(grep "MINOR" /pcpsrc/pcp/VERSION.pcp | cut -f2 -d=); export PCPREV=$(grep "REVISION" /pcpsrc/pcp/VERSION.pcp | cut -f2 -d=); cd /pcpsrc/pcp/pcp-$PCPMAJ.$PCPMIN.$PCPREV/build/rpm && rm ./*src.rpm && dnf install ./*rpm -y
#RUN the daemons
RUN systemctl enable pmcd pmlogger avahi-daemon
#RUN echo "-A" >> /etc/pcp/pmcd/pmcd.options
RUN echo "pcpqa   ALL=(ALL)       NOPASSWD: ALL" >> /etc/sudoers
RUN sed -i s/rlimit\-nproc=3/\#rlimit\-nproc=3/ /etc/avahi/avahi-daemon.conf
# workaround listed here: http://www.projectatomic.io/blog/2015/04/problems-with-ping-in-containers-on-atomic-hosts/
RUN setcap cap_net_raw,cap_net_admin+p /usr/bin/ping
CMD [ "/sbin/init" ]

