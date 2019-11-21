#!/bin/sh -eux
image_raw="build-${AZ_IMAGE}/${AZ_IMAGE}.img"
image_vhd="build-${AZ_IMAGE}/${AZ_IMAGE}.vhd"
image_vhd_url="${AZ_STORAGE}/${AZ_STORAGE_CONTAINER}/${AZ_IMAGE}.vhd"

qemu-img convert -f raw -o subformat=fixed,force_size -O vpc "${image_raw}" "${image_vhd}"

wget -q -O azcopy.tar.gz https://aka.ms/downloadazcopy-v10-linux
tar -xf azcopy.tar.gz --strip-components=1

AZCOPY_SPA_CLIENT_SECRET="${AZ_CLIENT_SECRET}" ./azcopy login --service-principal --application-id "${AZ_CLIENT_ID}" --tenant-id "${AZ_TENANT}"
./azcopy copy "${image_vhd}" "${image_vhd_url}"

az image create \
  --resource-group "${AZ_RESOURCE_GROUP}" \
  --name "${AZ_IMAGE}" \
  --os-type Linux \
  --source "${image_vhd_url}"

az storage blob delete \
  --container "${AZ_STORAGE_CONTAINER}" \
  --name "${AZ_IMAGE}.vhd"
