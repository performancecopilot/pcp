#!/bin/sh -eux
# https://docs.microsoft.com/en-us/azure/virtual-machines/linux/create-upload-centos

cat >> /etc/sysconfig/network  <<EOF
NETWORKING=yes
HOSTNAME=localhost.localdomain
EOF

cat > /etc/sysconfig/network-scripts/ifcfg-eth0 <<EOF
DEVICE=eth0
ONBOOT=yes
BOOTPROTO=dhcp
TYPE=Ethernet
USERCTL=no
PEERDNS=yes
IPV6INIT=no
NM_CONTROLLED=no
EOF

ln -s /dev/null /etc/udev/rules.d/75-persistent-net-generator.rules

sed -i 's/GRUB_CMDLINE_LINUX=".*"/GRUB_CMDLINE_LINUX="rootdelay=300 console=ttyS0 earlyprintk=ttyS0 net.ifnames=0"/' /etc/default/grub
grub2-mkconfig -o /boot/grub2/grub.cfg

echo 'add_drivers+=" hv_vmbus hv_netvsc hv_storvsc "' >> /etc/dracut.conf
dracut -f -v

dnf install -y python-pyasn1 WALinuxAgent
systemctl enable waagent
