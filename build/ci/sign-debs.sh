#!/bin/sh
#
# Sign and verify Debian packages for PCP releases.
#
# The secret key is read from $GNUPGHOME, which must be set.  Packages
# are signed in place (with debsigs) and then verified immediately.
# debsigs 0.2.2 or later is required: earlier versions do not support
# zstd-compressed packages and cannot verify signatures.
#
# Usage: sign-debs.sh [-c] [-v] [-k keyid] [-p passphrase-file] file ...
#
#   -c   verify only, do not sign
#   -v   verbose
#   -k   key id (default: $DEB_SIGN_KEYID or $RPM_GPG_KEYID,
#        else the first secret key)
#   -p   file containing the key passphrase
#
set -eu

prog=`basename "$0"`
keyid=${DEB_SIGN_KEYID:-${RPM_GPG_KEYID:-}}
passfile=
check=false
verbose=false

usage()
{
    echo >&2 "Usage: $prog [-c] [-v] [-k keyid] [-p passphrase-file] file ..."
    exit 1
}

die()
{
    echo >&2 "$prog: $*"
    exit 1
}

while getopts "cvk:p:" c
do
    case "$c"
    in
	c) check=true ;;
	v) verbose=true ;;
	k) keyid=$OPTARG ;;
	p) passfile=$OPTARG ;;
	*) usage ;;
    esac
done
shift `expr $OPTIND - 1`
[ $# -ge 1 ] || usage

[ -n "${GNUPGHOME:-}" ] || die "GNUPGHOME must be set"
command -v gpg >/dev/null 2>&1 || die "gpg not found"
command -v debsigs >/dev/null 2>&1 || die "debsigs not found"

if [ -z "$keyid" ]
then
    keyid=`gpg --batch --list-secret-keys --with-colons 2>/dev/null \
	| awk -F: '$1 == "fpr" { print $10; exit }'`
fi
[ -n "$keyid" ] || die "no secret key found in $GNUPGHOME"
gpg --batch --list-secret-keys "$keyid" >/dev/null 2>&1 \
    || die "secret key $keyid not found in $GNUPGHOME"

count=0
for file
do
    case "$file"
    in
	*.deb) ;;
	*) die "$file: not a Debian package" ;;
    esac
    [ -f "$file" ] || die "$file: no such file"

    if $check
    then
	if $verbose
	then
	    echo "Verifying $file"
	fi
    else
	if $verbose
	then
	    echo "Signing $file"
	fi
	if [ -n "$passfile" ]
	then
	    debsigs --sign=origin --default-key="$keyid" \
		--gpgopts="--batch --pinentry-mode loopback --passphrase-file $passfile" \
		"$file" || die "$file: debsigs signing failed"
	else
	    debsigs --sign=origin --default-key="$keyid" "$file" \
		|| die "$file: debsigs signing failed"
	fi
    fi

    debsigs --verify "$file" \
	|| die "$file: signature verification failed (debsigs 0.2.2 or later is required)"
    count=`expr $count + 1`
done

if $check
then
    echo "$prog: verified $count Debian package(s)"
else
    echo "$prog: signed and verified $count Debian package(s)"
fi
