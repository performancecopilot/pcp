#!/bin/sh
#
# Sign and verify RPM packages for PCP releases.
#
# The secret key is read from $GNUPGHOME, which must be set.  Packages
# are signed in place, then the signature is verified using a throwaway
# rpm keyring containing the matching public key.
#
# Usage: sign-rpms.sh [-c] [-v] [-k keyid] [-p passphrase-file] file ...
#
#   -c   verify only, do not sign
#   -v   verbose
#   -k   key id (default: $RPM_GPG_KEYID, else the first secret key)
#   -p   file containing the key passphrase
#
set -eu

prog=`basename "$0"`
keyid=${RPM_GPG_KEYID:-}
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
command -v rpm >/dev/null 2>&1 || die "rpm not found"
if ! $check
then
    command -v rpmsign >/dev/null 2>&1 || die "rpmsign not found"
fi

# Use an explicit key id where given, otherwise the only secret key in
# the (ephemeral) GNUPGHOME, which is what the release workflow imports.
#
if [ -z "$keyid" ]
then
    keyid=`gpg --batch --list-secret-keys --with-colons 2>/dev/null \
	| awk -F: '$1 == "fpr" { print $10; exit }'`
fi
[ -n "$keyid" ] || die "no secret key found in $GNUPGHOME"
gpg --batch --list-secret-keys "$keyid" >/dev/null 2>&1 \
    || die "secret key $keyid not found in $GNUPGHOME"

tmp=`mktemp -d`
trap 'rm -rf "$tmp"' 0 1 2 3 15

rpmdb="$tmp/rpmdb"
mkdir -p "$rpmdb"
rpm --dbpath "$rpmdb" --initdb >/dev/null 2>&1 \
    || die "cannot create a temporary rpm keyring"
gpg --batch --armor --export "$keyid" > "$tmp/pubkey.asc" \
    || die "cannot export the public key for $keyid"
rpm --dbpath "$rpmdb" --import "$tmp/pubkey.asc" \
    || die "cannot import the public key into the rpm keyring"

verify()
{
    [ -f "$1" ] || die "$1: no such file"
    output=`rpm --dbpath "$rpmdb" -Kv "$1" 2>&1` \
	|| die "$1: rpm signature check failed"
    # rpm reports an unsigned package as success for -K, so the
    # presence of an OK signature line is what matters here.
    #
    echo "$output" | grep -qi 'signature.*OK' \
	|| die "$1: no valid OpenPGP signature"
    if $verbose
    then
	echo "$output"
    fi
    return 0
}

count=0
for file
do
    case "$file"
    in
	*.rpm) ;;
	*) die "$file: not an rpm package" ;;
    esac

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
	    rpmsign --addsign \
		--define "__gpg $(command -v gpg)" \
		--define "_gpg_name $keyid" \
		--define "_gpg_path $GNUPGHOME" \
		--define "_gpg_sign_cmd_extra_args --batch --pinentry-mode loopback --passphrase-file $passfile" \
		"$file" || die "$file: rpmsign failed"
	else
	    rpmsign --addsign \
		--define "__gpg $(command -v gpg)" \
		--define "_gpg_name $keyid" \
		--define "_gpg_path $GNUPGHOME" \
		"$file" || die "$file: rpmsign failed"
	fi
    fi

    verify "$file"
    count=`expr $count + 1`
done

if $check
then
    echo "$prog: verified $count rpm package(s)"
else
    echo "$prog: signed and verified $count rpm package(s)"
fi
