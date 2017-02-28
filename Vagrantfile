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



############################################################
# Host Definititions
############################################################

pcp_hosts = {
        :ubuntu1204 => {
                :hostname => "ubuntu1204",
                :ipaddress => "10.100.10.10",
                :box => "ubuntu/precise64",
                :script => "ubuntu.sh"
        },
#        :ubuntu1304 => {
#                :hostname => "ubuntu1304",
#                :ipaddress => "10.100.10.11",
#                :box => "chef/ubuntu-13.04",
#                :script => "#{$script_EOLubuntu}"
#        },
        :ubuntu1404 => {
                :hostname => "ubuntu1404",
                :ipaddress => "10.100.10.12",
                :box => "ubuntu/trusty64",
                :script => "ubuntu.sh"
        },
#       :centos511 => {
#                :hostname => "centos511",
#                :ipaddress => "10.100.10.20",
#                :box => "chef/centos-5.11",
#                :script => "centos.sh"
#        },
# 	 :centos511_32 => {
#                :hostname => "centos511-32",
#                :ipaddress => "10.100.10.21",
#                :box => "chef/centos-5.11-i386",
#                :script => "centos.sh"
#        },
#        :centos65 => {
#               :hostname => "centos65",
#                :ipaddress => "10.100.10.22",
#                :box => "chef/centos-6.5",
#                :script => "centos.sh"
#        },
        :centos7 => {
                :hostname => "centos7",
                :ipaddress => "10.100.10.23",
                :box => "centos/7",
                :script => "centos.sh"
        },
        :fedora23 => {
                :hostname => "fedora23",
                :ipaddress => "10.100.10.30",
                :box => "fedora/23-cloud-base",
                :script => "fedora.sh"
        },
        :debian76 => {
                :hostname => "debianwheezy",
                :ipaddress => "10.100.10.40",
                :box => "debian/wheezy64",
                :script => "debian.sh"
        },
        :debian8 => {
                :hostname => "debianjessie",
                :ipaddress => "10.100.10.40",
                :box => "debian/jessie64",
                :script => "debian.sh"
        }
# Built locally from : https://github.com/opscode/bento
# NAT comes up but not the host interface
#        :opensuse132 => {
#                :hostname => "opensuse132",
#                :ipaddress => "10.100.10.51",
#                :box => "minnus-opensuse-13.2",
#                :script => "suse.sh"
#        }
# Networking wont come up
#        :opensuse131 => {
#                :hostname => "opensuse131",
#                :ipaddress => "10.100.10.50",
#                :box => "chef/opensuse-13.1",
#                :script => "suse.sh"
#        }
# Possible opensuse box?
#        :opensuseleap42 => {
#                :hostname => "opensuse42",
#                :ipaddress => "10.100.10.52",
#                :box => "opensuse/openSUSE-42.1-x86_64",
#                :script => "suse.sh"
#        }
}


EXPECTED_PLATFORM="darwin"
EXPECTED_ACK_FILE="provisioning/osxsierra.legally.ok"
platform_ok_to_use_OSX=RUBY_PLATFORM.include?(EXPECTED_PLATFORM)
eula_acknowledged = File.exists?(EXPECTED_ACK_FILE)
ok_to_use_OSX = platform_ok_to_use_OSX && eula_acknowledged

############################################################
# Only allowed to use oxssierra image if the underlying platform
# is a Mac and that the user has checked
# 
# see OSXREADME.md for more details
############################################################
if(ok_to_use_OSX)
	pcp_hosts[:osxsierra]={
					:hostname => "osxSierra",
					:ipaddress => "10.100.10.60",
					# https://github.com/AndrewDryga/vagrant-box-osx
					# TODO - this base image takes flipping ages to provision
					# due to PCP dependencies that need to be built by
					# Homebrew (eg. qt5, gettext)
					# it would be better to have a base image based off this
					# box that has these pre-installed
					:box => "http://files.dryga.com/boxes/osx-sierra-0.3.1.box",
					:script => "osxsierra.sh"
	}
end

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
  global_config.vm.synced_folder ".", "/vagrant", type: "rsync", rsync_auto: false, :rsync__exclude => ["qaresults/", "pcp-*"]

  pcp_hosts.each_pair do |name, options|
    global_config.vm.define name do |config|

       config.vm.provider "virtualbox" do |v|
          v.name = "Vagrant PCP - #{name}"
          v.customize ["modifyvm", :id, "--groups", "/VagrantPCP", "--memory", VM_MEM, "--cpus", VM_CPU]
       end

       config.vm.box = options[:box]

       # VM specific shared folder for qa results
       # config.vm.synced_folder "./qaresults/#{name}", "/qaresults", mount_options: ["dmode=777", "fmode=666"], create: true

       # TODO - this appears to fail with `vagrant provision osxsierra`
       config.vm.synced_folder "./qaresults/#{name}", "/qaresults", create: true

       config.vm.hostname = "#{options[:hostname]}"
       config.vm.network :private_network, ip: options[:ipaddress]

       # Setup networking etc
       config.vm.provision :shell, :inline => $script_common

       # Do platfrom specifics: install packages, etc
       config.vm.provision :shell, path: "provisioning/#{options[:script]}"

       # Run QA and copy results back to host
       config.vm.provision :shell, :path => "provisioning/qa.sh"
    end
  end
end
