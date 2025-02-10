#!/bin/sh
# Based on an (one file at a time) upload script here:
#https://github.com/danielmundi/upload-packagecloud/blob/main/push.sh

echo "Install dependencies"
sudo gem install package_cloud

user=performancecopilot
repo=pcp
here=`pwd`

# Iterate each of the builds
for build in artifacts/build-*
do
    [ $build = "artifacts/build-*" ] && continue

    # some examples...
    #	artifacts/build-fedora31-container
    #	artifacts/build-centos-stream9-container
    #	artifacts/build-centos6-container
    #	artifacts/build-debian11-container
    #	artifacts/build-ubuntu2204-container

    dist=`echo $build | sed -e 's/^.*build-//' -e 's/-container.*$//' | awk '
BEGIN {
    # map to any code names packagecloud expects
    dist["debian10"] = "debian/buster";
    dist["debian11"] = "debian/bullseye";
    dist["debian12"] = "debian/bookworm";
    dist["debian13"] = "debian/trixie";
    dist["debian14"] = "debian/forky";
    dist["ubuntu1804"] = "ubuntu/bionic";
    dist["ubuntu2004"] = "ubuntu/focal";
    dist["ubuntu2204"] = "ubuntu/jammy";
    dist["ubuntu2404"] = "ubuntu/noble";
}
$1 ~ /^debian/ || /^ubuntu/ { print dist[$1] }
$1 ~ /^fedora/ { match($1, /fedora([1-9][0-9]*$)/, m); printf "fedora/%s\n", m[1] }
$1 ~ /^centos/ { match($1, /cent.*([1-9][0-9]*$)/, m); printf "el/%s\n", m[1] }
'`
    [ -z "$dist" ] && continue

    echo "Upload $dist packages"
    cd $here/$build
    for file in `find . -name \*.rpm -o -name \*.deb | grep -Ev 'tests|\.src\.'`
    do
	echo package_cloud push $user/$repo/$dist/pcp $file
	package_cloud push $user/$repo/$dist/pcp $file
    done
done
