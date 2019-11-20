SSH="ssh -F scripts/vmss_ssh_config"

HOST_IPS=$(az vmss list-instance-public-ips \
  --resource-group "${AZ_RESOURCE_GROUP}" \
  --name "${AZ_VMSS}" \
  --query "[*].ipAddress" --output tsv)
BUILDER_IP=$(echo ${HOST_IPS} | tr ' ' '\n' | sort -n | head -n 1)
HOSTS_SSH="$(printf "$SSH pcp@%s," ${HOST_IPS})"
