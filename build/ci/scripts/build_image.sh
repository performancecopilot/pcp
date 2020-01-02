#!/bin/sh -eu

cd "$(dirname "$0")/.."
. scripts/env.sh

packer build -force "hosts/${CI_HOST}/image.json"
