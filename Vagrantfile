# -*- mode: ruby -*-
# vi: set ft=ruby :

# Leave this alone
VAGRANTFILE_API_VERSION = "2"

# VM Configs
VM_MEM = "1024"
VM_CPU = "1"

# QA Flags
QA_FLAGS = ""
#QA_FLAGS = "022"
#QA_FLAGS = "-g pmda.linux"


# Arch Specific Config Files

############################################################
# CentOS
############################################################
$script_centos = <<SCRIPT

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

SCRIPT

############################################################
# SUSE
############################################################
$script_suse = <<SCRIPT
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

SCRIPT

############################################################
# Ubuntu
############################################################
$script_ubuntu = <<SCRIPT
apt-get update
# Use a regex for some of the more obscure packages so apt doesn't exit if missing
# Trying to not have one script per box
apt-get install -y  '^(libreadline|libpapi|libpfm4|libcoin80|libicu)-dev$' \
	bc bison curl flex  git  g++  dpkg-dev  pkg-config  debhelper chrpath \
        python-all  python-all-dev  libnspr4-dev  libnss3-dev  libsasl2-dev  libmicrohttpd-dev  libavahi-common-dev \
        libqt4-dev  autotools-dev  autoconf  gawk  libxml-tokeparser-perl  libspreadsheet-read-perl gdb sysv-rc-conf \
	libcairo2-dev sysstat valgrind apache2 realpath unbound \
	libibumad-dev libsoqt-dev libsoqt-dev-common libnss3-tools libibmad-dev x11-utils

cd /vagrant
sudo -H -u vagrant ./Makepkgs
dpkg -i build/deb/*.deb

echo 'pcpqa   ALL=(ALL) NOPASSWD: ALL' >> /etc/sudoers

SCRIPT

############################################################
# Ubuntu EOL
############################################################
$script_EOLubuntu = <<SCRIPT

# Fix the repo names
sed -i.bak -r 's/(us.)?(archive|security).ubuntu.com/old-releases.ubuntu.com/g' /etc/apt/sources.list

apt-get update
apt-get install  -y '^(libreadline|libpapi|libpfm4|libcoin80|libicu)-dev$' \
	bc bison curl flex  git  g++  dpkg-dev  pkg-config  debhelper chrpath \
        python-all  python-all-dev  libnspr4-dev  libnss3-dev  libsasl2-dev  libmicrohttpd-dev  libavahi-common-dev \
        libqt4-dev  autotools-dev  autoconf  gawk  libxml-tokeparser-perl  libspreadsheet-read-perl gdb sysv-rc-conf \
	libcairo2-dev sysstat valgrind apache2 realpath unbound \
        libibumad-dev libsoqt-dev libsoqt-dev-common libnss3-tools libibmad-dev x11-utils

cd /vagrant
sudo -H -u vagrant ./Makepkgs
dpkg -i build/deb/*.deb

echo 'pcpqa   ALL=(ALL) NOPASSWD: ALL' >> /etc/sudoers

SCRIPT

############################################################
# Debian
############################################################
$script_debian = <<SCRIPT
apt-get update
apt-get install -y '^(libreadline|libpapi|libpfm4|libcoin80)-dev$' \
		bc bison  flex  git  g++  dpkg-dev  pkg-config  debhelper chrpath \
		python-all  python-all-dev  libnspr4-dev  libnss3-dev  libsasl2-dev  libmicrohttpd-dev  libavahi-common-dev \
		libqt4-dev  autotools-dev  autoconf  gawk  libxml-tokeparser-perl libspreadsheet-read-perl ed gdb sysv-rc-conf \
		libcairo2-dev libibumad-dev libibmad-dev sysstat valgrind apache2 realpath unbound libsoqt-dev \
		libsoqt-dev-common libnss3-tools x11-utils

cd /vagrant
sudo -H -u vagrant ./Makepkgs
dpkg -i build/deb/*.deb

echo 'pcpqa   ALL=(ALL) NOPASSWD: ALL' >> /etc/sudoers

SCRIPT

############################################################
# Fedora
############################################################
$script_fedora = <<SCRIPT

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

SCRIPT

############################################################
# Host Definititions
############################################################

pcp_hosts = {
        :ubuntu1204 => {
                :hostname => "ubuntu1204",
                :ipaddress => "10.100.10.10",
                :box => "ubuntu/precise64",
                :script => "#{$script_ubuntu}"
        },
        :ubuntu1304 => {
                :hostname => "ubuntu1304",
                :ipaddress => "10.100.10.11",
                :box => "chef/ubuntu-13.04",
                :script => "#{$script_EOLubuntu}"
        },
        :ubuntu1404 => {
                :hostname => "ubuntu1404",
                :ipaddress => "10.100.10.12",
                :box => "ubuntu/trusty64",
                :script => "#{$script_ubuntu}"
        },
        :centos511 => {
                :hostname => "centos511",
                :ipaddress => "10.100.10.20",
                :box => "chef/centos-5.11",
                :script => "#{$script_centos}"
        },
	:centos511_32 => {
                :hostname => "centos511-32",
                :ipaddress => "10.100.10.21",
                :box => "chef/centos-5.11-i386",
                :script => "#{$script_centos}"
        },
        :centos65 => {
                :hostname => "centos65",
                :ipaddress => "10.100.10.22",
                :box => "chef/centos-6.5",
                :script => "#{$script_centos}"
        },
        :centos7 => {
                :hostname => "centos7",
                :ipaddress => "10.100.10.23",
                :box => "chef/centos-7.0",
                :script => "#{$script_centos}"
        },
        :fedora19 => {
                :hostname => "fedora19",
                :ipaddress => "10.100.10.30",
                :box => "chef/fedora-19",
                :script => "#{$script_fedora}"
        },
        :fedora20 => {
                :hostname => "fedora20",
                :ipaddress => "10.100.10.31",
                :box => "chef/fedora-20",
                :script => "#{$script_fedora}"
        },
        :debian76 => {
                :hostname => "debian76",
                :ipaddress => "10.100.10.40",
                :box => "chef/debian-7.6",
                :script => "#{$script_debian}"
        }
# Built locally from : https://github.com/opscode/bento
# NAT comes up but not the host interface
#        :opensuse132 => {
#                :hostname => "opensuse132",
#                :ipaddress => "10.100.10.51",
#                :box => "minnus-opensuse-13.2",
#                :script => "#{$script_suse}"
#        }
# Networking wont come up
#        :opensuse131 => {
#                :hostname => "opensuse131",
#                :ipaddress => "10.100.10.50",
#                :box => "chef/opensuse-13.1",
#                :script => "#{$script_suse}"
#        }
}

############################################################
# Common Config Setup, hostnames, etc
# So VMs could talk to each other if we wanted
############################################################

$script_common = ""

# We set our own IP
$script_common << "sed -i '/127.0.1.1/d' /etc/hosts\n"
# Fix bogus entries that some os use
$script_common << "sed -i '/127.0.0.1/d' /etc/hosts\n"
$script_common << "echo \"127.0.0.1 localhost.localdomain localhost\" >> /etc/hosts\n"

pcp_hosts.each_pair do |name, options|
        ipaddr = options[:ipaddress]
	hostname = options[:hostname]
        $script_common << "echo \"#{ipaddr} #{hostname} #{hostname}.localdomain\" >> /etc/hosts\n"
end

$script_common << "domainname localdomain"

############################################################
# QA run script
############################################################

$script_qa = <<SCRIPT

touch /tmp/runqa.sh
echo "#!/bin/sh" >> /tmp/runqa.sh
echo "cd /var/lib/pcp/testsuite" >> /tmp/runqa.sh
echo "./check #{QA_FLAGS} >/tmp/runqa.out 2>&1" >> /tmp/runqa.sh
echo "cp /tmp/runqa.out /qaresults" >> /tmp/runqa.sh
echo "cp /var/lib/pcp/testsuite/*.bad /qaresults" >> /tmp/runqa.sh

chmod 777 /tmp/runqa.sh
sudo -b -H -u pcpqa sh -c '/tmp/runqa.sh'

SCRIPT

############################################################
# Main Vagrant Init : Loop over Host configs
############################################################

Vagrant.configure(VAGRANTFILE_API_VERSION) do |global_config|

  global_config.ssh.forward_x11 = true
  global_config.ssh.forward_agent = true
  global_config.ssh.insert_key = false


  if Vagrant.has_plugin?("vagrant-cachier")
    global_config.cache.scope = :box
  end

  # Global shared folder for pcp source.  Copy it so we have our own to muck around in
  global_config.vm.synced_folder ".", "/vagrant", type: "rsync", rsync_auto: false, :rsync__exclude => ["qaresults/"]

  pcp_hosts.each_pair do |name, options|

	global_config.vm.define name do |config|

  	   config.vm.provider "virtualbox" do |v|
		v.name = "Vagrant PCP - #{name}"
    		v.customize ["modifyvm", :id, "--groups", "/VagrantPCP", "--memory", VM_MEM, "--cpus", VM_CPU]
  	   end

	   config.vm.box = options[:box]
	   
	   # VM specific shared folder for qa results
  	   config.vm.synced_folder "./qaresults/#{name}", "/qaresults", mount_options: ["dmode=777", "fmode=666"], create: true

	   #config.vm.hostname = "#{name}"
	   config.vm.hostname = "#{options[:hostname]}"
           config.vm.network :private_network, ip: options[:ipaddress]

	   # Setup networking etc
           config.vm.provision :shell, :inline => $script_common

	   # Do platfrom specifics: install packages, etc
           config.vm.provision :shell, :inline => options[:script]

	   # Run QA and copy results back to host
           config.vm.provision :shell, :inline => $script_qa
	end
  end
end
