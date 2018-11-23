# -*- mode: ruby -*-
# vi: set ft=ruby :

# Leave this alone
VAGRANTFILE_API_VERSION = "2"

# General VM Parameters - 8Gb and 4CPUs
VM_MEM = "8192"
VM_CPU = "4"

# QA Flags for provisioning/qa.sh
QA_FLAGS = ""
#QA_FLAGS = "022"
#QA_FLAGS = "-g pmda.linux"



############################################################
# Host Definititions
############################################################

pcp_hosts = {
        :ubuntu1804 => {
                :hostname => "ubuntu1804",
                :ipaddress => "10.100.10.24",
                :box => "generic/ubuntu1804",
                :script => "ubuntu.sh"
        },
        :ubuntu1604 => {
                :hostname => "ubuntu1604",
                :ipaddress => "10.100.10.23",
                :box => "generic/ubuntu1604",
                :script => "ubuntu.sh"
        },
        :ubuntu1404 => {
                :hostname => "ubuntu1404",
                :ipaddress => "10.100.10.22",
                :box => "generic/ubuntu1404",
                :script => "ubuntu.sh"
        },
        :rhel7 => {
                :hostname => "centos7",
                :ipaddress => "10.100.10.21",
                :box => "generic/centos7",
                :script => "rhel.sh"
        },
        :centos7 => {
                :hostname => "centos7",
                :ipaddress => "10.100.10.20",
                :box => "generic/centos7",
                :script => "centos.sh"
        },
        :centos6 => {
               :hostname => "centos6",
                :ipaddress => "10.100.10.19",
                :box => "generic/centos6",
                :script => "centos.sh"
        },
        :fedora29 => {
                :hostname => "fedora29",
                :ipaddress => "10.100.10.18",
                :box => "generic/fedora29",
                :script => "fedora.sh"
        },
        :fedora28 => {
                :hostname => "fedora28",
                :ipaddress => "10.100.10.17",
                :box => "generic/fedora28",
                :script => "fedora.sh"
        },
        :fedora27 => {
                :hostname => "fedora27",
                :ipaddress => "10.100.10.16",
                :box => "generic/fedora27",
                :script => "fedora.sh"
        },
        :fedora26 => {
                :hostname => "fedora26",
                :ipaddress => "10.100.10.15",
                :box => "generic/fedora26",
                :script => "fedora.sh"
        },
        :fedora25 => {
                :hostname => "fedora25",
                :ipaddress => "10.100.10.14",
                :box => "generic/fedora25",
                :script => "fedora.sh"
        },
        :debian8 => {
                :hostname => "debian8",
                :ipaddress => "10.100.10.13",
                :box => "generic/debian8",
                :script => "debian.sh"
        },
        :debian7 => {
                :hostname => "debian7",
                :ipaddress => "10.100.10.12",
                :box => "generic/debian7",
                :script => "debian.sh"
        },
        :freebsd12 => {
                :hostname => "freebsd12",
                :ipaddress => "10.100.10.11",
                :box => "generic/openbsd12",
                :script => "openbsd.sh"
        },
        :freebsd11 => {
                :hostname => "freebsd11",
                :ipaddress => "10.100.10.10",
                :box => "generic/freebsd11",
                :script => "openbsd.sh"
        },
        :openbsd6 => {
                :hostname => "openbsd6",
                :ipaddress => "10.100.10.9",
                :box => "generic/openbsd6",
                :script => "openbsd.sh"
        },
        :opensuse42 => {
                :hostname => "opensuse42",
                :ipaddress => "10.100.10.8",
                :box => "generic/opensuse42",
                :script => "opensuse.sh"
        }
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
