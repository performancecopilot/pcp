#!/bin/sh -eu

PREV_PWD=$PWD
cd "$(dirname "$0")/.."
. scripts/env.sh
. scripts/vmss.env.sh

$SSH pcp@${BUILDER_IP} "GIT_REPO=${GIT_REPO} GIT_COMMIT=${GIT_COMMIT} /usr/local/ci/build.sh"
