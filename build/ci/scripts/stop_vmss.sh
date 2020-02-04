#!/bin/sh -eu

cd "$(dirname "$0")/.."
. scripts/env.sh

az vmss delete \
  --resource-group "${AZ_RESOURCE_GROUP}" \
  --name "${AZ_VMSS}" \
  --no-wait
