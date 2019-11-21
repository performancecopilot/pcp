if [ $# -lt 1 ]; then
    echo "Usage: $0 <host>"
    exit 1
fi

CI_HOST="$1"
AZ_RESOURCE_GROUP="PCP_Builds"
AZ_LOCATION="eastus"
AZ_VM_SIZE="Standard_F2s_v2"
AZ_IMAGE="image-${CI_HOST}"
AZ_PLAN_INFO=""
[ ! -z ${BUILD_ID+x} ] && AZ_VMSS="build-${BUILD_ID}-${CI_HOST}"
AZ_STORAGE="https://pcpstore.blob.core.windows.net"
AZ_STORAGE_CONTAINER="vhd-images"

if [ -e "hosts/${CI_HOST}/env.sh" ]; then
    . "hosts/${CI_HOST}/env.sh"
fi
