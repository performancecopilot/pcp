#!/bin/sh -eu

cd "$(dirname "$0")/.."
. scripts/env.sh
. scripts/env.build.sh

az image show \
  --resource-group "${AZ_RESOURCE_GROUP}" \
  --name "${AZ_IMAGE}" \
  --query tags

az vmss create \
  --resource-group "${AZ_RESOURCE_GROUP}" \
  --name "${AZ_VMSS}" \
  --vm-sku "${AZ_VM_SIZE}" \
  --computer-name-prefix "${AZ_VMSS}-inst" \
  --instance-count "$2" \
  --image "${AZ_IMAGE}" \
  --lb "" \
  --public-ip-per-vm \
  --admin-username pcp \
  ${AZ_PLAN_INFO}
