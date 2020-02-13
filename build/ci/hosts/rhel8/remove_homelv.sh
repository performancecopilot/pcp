#!/bin/sh -eux

cp -a /home /home2
umount /dev/rootvg/homelv
lvremove -y /dev/rootvg/homelv
sed -i '/rootvg-homelv/d' /etc/fstab
rmdir /home
mv /home2 /home
