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
EOF

ln -s /dev/null /etc/udev/rules.d/75-persistent-net-generator.rules

touch /etc/cloud/cloud-init.disabled
rm -rf /var/lib/cloud/instances

sed -i 's/GRUB_CMDLINE_LINUX=".*"/GRUB_CMDLINE_LINUX="console=tty1 console=ttyS0 earlyprintk=ttyS0 rootdelay=300 net.ifnames=0"/' /etc/default/grub
sed -i 's/GRUB_TERMINAL_OUTPUT=".*"/GRUB_TERMINAL="serial console"/' /etc/default/grub
echo 'GRUB_SERIAL_COMMAND="serial --speed=115200 --unit=0 --word=8 --parity=no --stop=1"' >> /etc/default/grub
grub2-mkconfig -o /boot/grub2/grub.cfg

echo 'add_drivers+=" hv_vmbus hv_netvsc hv_storvsc "' >> /etc/dracut.conf
dracut -f -v

yum install -y python-pyasn1 WALinuxAgent
systemctl enable waagent
