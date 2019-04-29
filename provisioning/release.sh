#!/bin/sh
cd /vagrant || exit
# could probably do both steps in one via awk
sed -i -e '/distro.*/d' .bintrayrc
echo "distro=\"$1\"" >> .bintrayrc

printf 'n\ny\n' | ./scripts/bintray-upload

