#!/bin/sh

apt-get update
apt-get install -y '^(libreadline|libpapi|libpfm4|libcoin80)-dev$' \
		bc bison  flex  git  g++  dpkg-dev  pkg-config  debhelper chrpath \
		python-all  python-all-dev  libnspr4-dev  libnss3-dev  libsasl2-dev  libmicrohttpd-dev  libavahi-common-dev \
		libqt4-dev  autotools-dev  autoconf  gawk  libxml-tokeparser-perl libspreadsheet-read-perl ed gdb sysv-rc-conf \
		libcairo2-dev libibumad-dev libibmad-dev sysstat valgrind apache2 realpath unbound libsoqt-dev \
		libsoqt-dev-common libnss3-tools x11-utils build-essential librrds-perl

cd /vagrant
sudo -H -u vagrant ./Makepkgs
dpkg -i build/deb/*.deb

echo 'pcpqa   ALL=(ALL) NOPASSWD: ALL' >> /etc/sudoers
