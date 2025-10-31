#!/usr/bin/env bash

set -eu

usage () {
    echo "USAGE: ./update-mailmap.sh <bpftool-repo> <linux-repo>"
    exit 1
}

BPFTOOL_REPO=${1-""}
LINUX_REPO=${2-""}

if [ -z "${BPFTOOL_REPO}" ] || [ -z "${LINUX_REPO}" ]; then
    echo "Error: bpftool or linux repos are not specified"
    usage
fi

BPFTOOL_MAILMAP="${BPFTOOL_REPO}/.mailmap"
LINUX_MAILMAP="${LINUX_REPO}/.mailmap"

tmpfile="$(mktemp)"
cleanup() {
    rm -f "${tmpfile}"
}
trap cleanup EXIT

grep_lines() {
    local pattern="$1"
    local file="$2"
    grep "${pattern}" "${file}" || true
}

while read -r email; do
    grep_lines "${email}$" "${LINUX_MAILMAP}" >> "${tmpfile}"
done < <(git log --format='<%ae>' | sort -u)

sort -u "${tmpfile}" > "${BPFTOOL_MAILMAP}"
