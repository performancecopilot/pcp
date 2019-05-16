FROM debian:testing
SHELL ["/bin/bash", "-c"]

RUN apt-get update && apt-get install -y \
        pkg-config make gcc g++ libc6-dev flex bison \
        sudo hostname findutils bc git cppcheck \
        man procps \
        perl  \
        python2-dev python3 python3-dev pylint \
        e2fsprogs xfsprogs systemd-sysv \
        dpkg-dev gawk debhelper libreadline-dev chrpath python-all python3-all python-dev \
        libnspr4-dev libnss3-dev libsasl2-dev libmicrohttpd-dev libavahi-common-dev \
        zlib1g-dev libclass-dbi-perl libdbd-mysql-perl python3-psycopg2 libcairo2-dev \
        libextutils-autoinstall-perl libxml-tokeparser-perl librrds-perl libjson-perl \
        libwww-perl libnet-snmp-perl libnss3-tools \
        python3-requests libspreadsheet-read-perl libdata-peek-perl time \
        python3-bpfcc libuv1-dev libssl-dev

RUN useradd -c "Performance Co-Pilot" -d /var/lib/pcp -M -r -s /usr/sbin/nologin pcp
COPY . /pcp

RUN cd /pcp && ./Makepkgs
RUN mkdir /packages && cd /packages && mv /pcp/build/deb/*.deb . && dpkg -i *.deb
RUN sed -ie '/\[Service\]/aTimeoutSec=120' /lib/systemd/system/{pmcd,pmlogger,pmie,pmproxy}.service
CMD ["/sbin/init"]
