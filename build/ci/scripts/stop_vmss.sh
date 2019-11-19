#!/bin/sh -eu

cd "$(dirname "$0")/.."
. common/env.sh

az vmss delete \
  --resource-group "${AZ_RESOURCE_GROUP}" \
  --name "${AZ_VMSS}" \
  --no-wait
