#!/bin/sh -eu

cd "$(dirname "$0")/.."
. scripts/env.sh

echo Loading old deployments
deployments="$(az deployment group list \
  --resource-group "${AZ_RESOURCE_GROUP}" \
  --query "[].name" \
  --output tsv)"

for deployment in ${deployments}
do
  echo Deleting deployment "${deployment}"
  az deployment group delete \
    --resource-group "${AZ_RESOURCE_GROUP}" \
    --name "${deployment}" \
    --no-wait
done
