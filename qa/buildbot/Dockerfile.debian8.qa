FROM debian:jessie
MAINTAINER PCP Development Team <pcp@groups.io>
ENV container docker
STOPSIGNAL SIGRTMIN+3
RUN sed -i 'p' /etc/apt/sources.list && sed -i ':a;N;$!ba;s/deb /deb-src /2' /etc/apt/sources.list &&  sed -i ':a;N;$!ba;s/deb /deb-src /3' /etc/apt/sources.list && sed -i ':a;N;$!ba;s/deb /deb-src /4' /etc/apt/sources.list
RUN apt-get update && apt-get upgrade -y && apt-get install -y git vim bc time libspreadsheet-read-perl avahi-daemon sudo devscripts chkconfig && apt-get clean all
RUN apt-get build-dep -y pcp
RUN mkdir -p /pcpsrc/ && cd /pcpsrc/ && git clone git://git.pcp.io/pcp/pcp.git && cd pcp && ./configure --prefix=/usr --libexecdir=/usr/lib --sysconfdir=/etc --localstatedir=/var --with-rcdir=/etc/init.d --with-sysconfigdir=/etc/default --without-systemd
RUN apt-get update && cd /pcpsrc/pcp/debian && ./fixcontrol.master > control && echo "y" | mk-build-deps --install /pcpsrc/pcp/debian/control
RUN cd /pcpsrc/pcp/ && ./Makepkgs --verbose
RUN cd /pcpsrc/pcp/build/deb && apt-get install ./*.deb

RUN systemctl enable pmcd pmlogger

RUN echo "pcpqa   ALL=(ALL)       NOPASSWD: ALL" >> /etc/sudoers
RUN sed -i s/rlimit\-nproc=3/\#rlimit\-nproc=3/ /etc/avahi/avahi-daemon.conf
# workaround listed here: http://www.projectatomic.io/blog/2015/04/problems-with-ping-in-containers-on-atomic-hosts/
RUN setcap cap_net_raw,cap_net_admin+p /usr/bin/ping

CMD [ "/sbin/init" ]
