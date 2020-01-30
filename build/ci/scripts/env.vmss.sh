SSH="ssh -F scripts/vmss_ssh_config"

AZ_VMSS_IPS=$(az vmss list-instance-public-ips \
  --resource-group "${AZ_RESOURCE_GROUP}" \
  --name "${AZ_VMSS}" \
  --query "[*].ipAddress" --output tsv)
AZ_VMSS_BUILDER_IP=$(echo ${AZ_VMSS_IPS} | tr ' ' '\n' | sort -n | head -n 1)
AZ_VMSS_HOSTS_SSH="$(printf "$SSH pcp@%s," ${AZ_VMSS_IPS})"
