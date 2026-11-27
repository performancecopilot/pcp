#!/bin/sh
#
# Self-test for the release artifact signing scripts.
#
# Builds a throwaway RPM and DEB, generates an ephemeral passphrase
# protected key, signs both packages and verifies the results.  Also
# checks that unsigned packages are rejected.
#
# Intended to run inside the signing container from signing.Dockerfile:
#
#   docker run --rm -v "$PWD:/src:ro" pcp-signing /src/build/ci/sign-test.sh
#
set -eu

here=`dirname "$0"`
case "$here"
in
/*) ;;
*) here=`pwd`/$here ;;
esac

tmp=`mktemp -d`
trap 'rm -rf "$tmp"' 0 1 2 3 15

export GNUPGHOME="$tmp/gnupg"
mkdir -m 700 "$GNUPGHOME"

passphrase=pcp-sign-test
( umask 077; printf '%s' "$passphrase" > "$tmp/pass" )

echo "Generating ephemeral signing key ..."
gpg --batch --pinentry-mode loopback --passphrase "$passphrase" \
    --quick-generate-key 'PCP Signing Test <sign-test@example.com>' \
    rsa3072 sign never >/dev/null 2>&1

echo "Building test packages ..."
mkdir -p "$tmp/rpm/SPECS" "$tmp/deb/DEBIAN" "$tmp/deb/usr/share/pcp-sign-test"
cat > "$tmp/rpm/SPECS/pcp-sign-test.spec" <<'EOF'
Name: pcp-sign-test
Version: 1
Release: 1
Summary: PCP signing test package
License: GPL-2.0-or-later
BuildArch: noarch
%description
Throwaway package used by the PCP release signing self-test.
%install
mkdir -p %{buildroot}/usr/share/pcp-sign-test
%files
/usr/share/pcp-sign-test
EOF
rpmbuild -bb --define "_topdir $tmp/rpm" \
    "$tmp/rpm/SPECS/pcp-sign-test.spec" >/dev/null 2>&1

cat > "$tmp/deb/DEBIAN/control" <<'EOF'
Package: pcp-sign-test
Version: 1-1
Section: misc
Priority: optional
Architecture: all
Maintainer: PCP Signing Test <sign-test@example.com>
Description: PCP signing test package
 Throwaway package used by the PCP release signing self-test.
EOF
dpkg-deb -Zzstd --build "$tmp/deb" "$tmp/pcp-sign-test.deb" >/dev/null

rpmfile=`echo "$tmp"/rpm/RPMS/noarch/pcp-sign-test-*.rpm`
debfile="$tmp/pcp-sign-test.deb"
cp "$rpmfile" "$tmp/unsigned.rpm"
cp "$debfile" "$tmp/unsigned.deb"

echo "Signing packages ..."
"$here/sign-rpms.sh" -p "$tmp/pass" "$rpmfile"
"$here/sign-debs.sh" -p "$tmp/pass" "$debfile"

echo "Re-verifying signed packages ..."
"$here/sign-rpms.sh" -c "$rpmfile"
"$here/sign-debs.sh" -c "$debfile"

echo "Checking unsigned packages are rejected ..."
if "$here/sign-rpms.sh" -c "$tmp/unsigned.rpm" >/dev/null 2>&1
then
    echo >&2 "sign-test: unsigned rpm was accepted"
    exit 1
fi
if "$here/sign-debs.sh" -c "$tmp/unsigned.deb" >/dev/null 2>&1
then
    echo >&2 "sign-test: unsigned deb was accepted"
    exit 1
fi

echo "Checking signed packages are usable ..."
if [ `id -u` -eq 0 ]
then
    dpkg -i "$debfile" >/dev/null
    dpkg --purge pcp-sign-test >/dev/null
else
    dpkg-deb -x "$debfile" "$tmp/extract"
    test -d "$tmp/extract/usr/share/pcp-sign-test"
fi

echo "sign-test: all signing tests passed"
