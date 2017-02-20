#!/bin/sh
# Fix the repo names
sed -i.bak -r 's/(us.)?(archive|security).ubuntu.com/old-releases.ubuntu.com/g' /etc/apt/sources.list

apt-get update
apt-get install  -y '^(libreadline|libpapi|libpfm4|libcoin80|libicu)-dev$' \
	bc bison curl flex  git  g++  dpkg-dev  pkg-config  debhelper chrpath \
        python-all  python-all-dev  libnspr4-dev  libnss3-dev  libsasl2-dev  libmicrohttpd-dev  libavahi-common-dev \
        libqt4-dev  autotools-dev  autoconf  gawk  libxml-tokeparser-perl  libspreadsheet-read-perl gdb sysv-rc-conf \
	libcairo2-dev sysstat valgrind apache2 realpath unbound \
        libibumad-dev libsoqt-dev libsoqt-dev-common libnss3-tools libibmad-dev x11-utils build-essential \
        librrds-perl

cd /vagrant
sudo -H -u vagrant ./Makepkgs
dpkg -i build/deb/*.deb

echo 'pcpqa   ALL=(ALL) NOPASSWD: ALL' >> /etc/sudoers
