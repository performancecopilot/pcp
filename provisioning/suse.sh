#!/bin/sh

zypper -n in -t pattern devel_basis devel_C_C++ devel_rpm_build
zypper -n in git ncurses-devel readline-devel man libmicrohttpd-devel libqt4-devel perl-JSON sysstat perl-TimeDate perl-ExtUtils-MakeMaker bc \
		cyrus-sasl-devel systemd-devel avahi-devel rpm-devel  perl-Text-CSV_XS \
		bind-utils python-devel python-curses

# These dont have the right "provides" in opensuse
# libibumad-devel libibmad-devel papi-devel libpfm-devel

#SUSE doesn't get the right perms in /vagrant, hmm
chown -R vagrant /vagrant

cd /vagrant
sudo -H -u vagrant ./Makepkgs
rpm -ivh  pcp-*/build/rpm/*.rpm

# Doesn't start automatically on all distributions
/sbin/service pmcd start

echo 'pcpqa   ALL=(ALL) NOPASSWD: ALL' >> /etc/sudoers
