#!/bin/sh -eu

cd "$(dirname "$0")/.."
. common/env.sh

AZ_RESOURCE_GROUP="${AZ_RESOURCE_GROUP}" AZ_LOCATION="${AZ_LOCATION}" AZ_VM_SIZE="${AZ_VM_SIZE}" AZ_IMAGE="${AZ_IMAGE}" \
  GIT_REPO="${GIT_REPO}" GIT_COMMIT="${GIT_COMMIT}" \
  packer build -force "hosts/${CI_HOST}/image.json"
