#!/bin/sh -xe

az vmss create \
  --resource-group "${RESOURCE_GROUP}" \
  --name "${VMSS}" \
  --computer-name-prefix "${VMSS}-agent" \
  --instance-count 2 \
  --image "${IMAGE}" \
  --lb "" \
  --public-ip-per-vm \
  --admin-username pcp \
  --tags "BUILD_ID=${BUILD_ID} PCP_COMMIT=${PCP_COMMIT}"
