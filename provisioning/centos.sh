#!/bin/sh

# Setup EPEL
cat <<EOM >/etc/yum.repos.d/epel-bootstrap.repo
[epel]
name=Bootstrap EPEL
mirrorlist=http://mirrors.fedoraproject.org/mirrorlist?repo=epel-\\\$releasever&arch=\\\$basearch
failovermethod=priority
enabled=0
gpgcheck=0
EOM

yum --enablerepo=epel -y install epel-release
rm -f /etc/yum.repos.d/epel-bootstrap.repo

yum -y groupinstall 'Development Tools'
yum -y install git ncurses-devel readline-devel man libmicrohttpd-devel qt4-devel python26 python26-devel perl-JSON sysstat perl-TimeDate \
		perl-XML-TokeParser perl-ExtUtils-MakeMaker perl-Time-HiRes systemd-devel bc cairo-devel cyrus-sasl-devel \
		systemd-devel libibumad-devel libibmad-devel papi-devel libpfm-devel rpm-devel perl-Spreadsheet-WriteExcel \
		perl-Text-CSV_XS bind-utils httpd python-devel nspr-devel nss-devel python-ctypes nss-tools perl-Spreadsheet-XLSX \
		ed cpan valgrind time xdpyinfo rrdtool-perl

# Lots of avahi errors on Centos511, likely due to my environment
# avahi-devel

cd /vagrant
sudo -H -u vagrant env PYTHON=python2.6 ./Makepkgs
rpm -ivh  pcp-*/build/rpm/*.rpm

# Doesn't start automatically on all distributions
/sbin/service pmcd start

echo 'pcpqa   ALL=(ALL) NOPASSWD: ALL' >> /etc/sudoers