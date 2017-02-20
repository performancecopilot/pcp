#!/bin/sh

# For sudo to work from a script
sed -i '/requiretty/d' /etc/sudoers 

yum -y group install 'Development Tools' 'C Development Tools and Libraries' 'RPM Development Tools'
yum -y install perl-ExtUtils-MakeMaker bison flex libmicrohttpd-devel qt-devel fedora-packager systemd-devel perl-JSON \
		sysstat perl-Digest-MD5 bc ed cpan cairo-devel cyrus-sasl-devel libibumad-devel libibmad-devel avahi-devel \
		papi-devel libpfm-devel rpm-devel perl-TimeDate perl-XML-TokeParser perl-Spreadsheet-WriteExcel \
		perl-Text-CSV_XS bind-utils httpd python-devel nspr-devel nss-devel perl-Spreadsheet-XLSX time xorg-x11-utils

# Remove too old libraries
pfmvers=`rpm -q --qf "%{VERSION}\n" libpfm`
[ "$pfmvers" = "`echo -e "$pfmvers\n4.3.9" | sort -V | head -n1`" ] && rpm -e libpfm libpfm-devel papi papi-devel

cd /vagrant
sudo -H -u vagrant ./Makepkgs
rpm -ivh  pcp-*/build/rpm/*.rpm

# Doesn't start automatically on all distributions
/sbin/service pmcd start

echo 'pcpqa   ALL=(ALL) NOPASSWD: ALL' >> /etc/sudoers
