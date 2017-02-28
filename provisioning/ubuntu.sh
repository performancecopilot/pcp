#!/bin/sh
apt-get update
# Use a regex for some of the more obscure packages so apt doesn't exit if missing
# Trying to not have one script per box
apt-get install -y  '^(libreadline|libpapi|libpfm4|libcoin80|libicu)-dev$' \
	bc bison curl flex  git  g++  dpkg-dev  pkg-config  debhelper chrpath \
        python-all  python-all-dev  libnspr4-dev  libnss3-dev  libsasl2-dev  libmicrohttpd-dev  libavahi-common-dev \
        libqt4-dev  autotools-dev  autoconf  gawk  libxml-tokeparser-perl  libspreadsheet-read-perl gdb sysv-rc-conf \
	libcairo2-dev sysstat valgrind apache2 realpath unbound \
	libibumad-dev libsoqt-dev libsoqt-dev-common libnss3-tools libibmad-dev x11-utils build-essential pbuilder

cd /vagrant
sudo -H -u vagrant ./Makepkgs
dpkg -i build/deb/*.deb

echo 'pcpqa   ALL=(ALL) NOPASSWD: ALL' >> /etc/sudoers
