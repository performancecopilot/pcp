#!/bin/sh
# Based on an (one file at a time) upload script here:
#https://github.com/danielmundi/upload-packagecloud/blob/main/push.sh

set -e

echo "Install dependencies"
sudo gem install package_cloud

#user=performancecopilot
user=natoscott
repo=pcp
here=`pwd`

# Iterate each of the builds
for build in artifacts/build-*
do
    [ $build = "artifacts/build-*" ] && continue

    # ex. artifacts/build-fedora31-container
    dist=`echo $build | sed -e 's/^.*build-//' -e 's/-container$//' | awk '
BEGIN {
    # map to any code names packagecloud expects
    dist["debian10"] = "debian/buster";
    dist["debian11"] = "debian/bullseye";
    dist["debian12"] = "debian/bookworm";
    dist["debian13"] = "debian/trixie";
    dist["ubuntu1804"] = "ubuntu/bionic";
    dist["ubuntu2004"] = "ubuntu/focal";
    dist["ubuntu2204"] = "ubuntu/jammy";
}
$1 ~ /^debian/ || /^ubuntu/ { print dist[$1] }
$1 ~ /^fedora/ { match($1, /f.*([1-9][0-9]*)/, m); printf "fedora/%s\n", m[1] }
$1 ~ /^centos/ { match($1, /c.*([1-9][0-9]*)/, m); printf "el/%s\n", m[1] }
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
