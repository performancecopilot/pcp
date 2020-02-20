#!/bin/sh -eux
image_raw="build-${AZ_IMAGE}/${AZ_IMAGE}.img"
image_vhd="build-${AZ_IMAGE}/${AZ_IMAGE}.vhd"
image_vhd_url="$(az storage blob url \
  --account-name "${AZ_STORAGE_ACCOUNT}" \
  --container-name "${AZ_STORAGE_CONTAINER}" \
  --name "${AZ_IMAGE}.vhd" \
  --output tsv)"

# Azure VM images need to be aligned at 1MB
MB=$((1024*1024))
size=$(qemu-img info -f raw --output json "${image_raw}" | jq '."virtual-size"')
rounded_size=$(((size/MB + 1)*MB))
qemu-img resize -f raw "${image_raw}" ${rounded_size}

qemu-img convert -f raw -o subformat=fixed,force_size -O vpc "${image_raw}" "${image_vhd}"
azcopy copy "${image_vhd}" "${image_vhd_url}"

# create does not overwrite the image if the source URL matches
az image delete \
  --resource-group "${AZ_RESOURCE_GROUP}" \
  --name "${AZ_IMAGE}"

az image create \
  --resource-group "${AZ_RESOURCE_GROUP}" \
  --name "${AZ_IMAGE}" \
  --source "${image_vhd_url}" \
  --os-type Linux \
  --os-disk-caching ReadWrite \
  --tags "created_at=$(date -u -Iseconds)" "git_repo=${GIT_REPO}" "git_commit=${GIT_COMMIT}"

az storage blob delete \
  --account-name "${AZ_STORAGE_ACCOUNT}" \
  --container-name "${AZ_STORAGE_CONTAINER}" \
  --name "${AZ_IMAGE}.vhd"
