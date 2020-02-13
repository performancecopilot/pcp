if [ $# -lt 1 ]; then
    echo "Usage: $0 <host>"
    exit 1
fi

CI_HOST="$1"

. ../../VERSION.pcp
PCP_VERSION=${PACKAGE_MAJOR}.${PACKAGE_MINOR}.${PACKAGE_REVISION}
PCP_BUILD_VERSION=${PCP_VERSION}-${PACKAGE_BUILD}

# export env vars required by packer
export AZ_RESOURCE_GROUP="PCP_Builds"
export AZ_LOCATION="eastus"
export AZ_STORAGE_ACCOUNT="pcpstore"
export AZ_STORAGE_CONTAINER="vhd-images"
export AZ_VM_SIZE="Standard_B2s"
export AZ_IMAGE="image-${CI_HOST}"
AZ_VMSS="build-${PCP_BUILD_VERSION}-${CI_HOST}"
AZ_PLAN_INFO=""

BINTRAY_SUBJECT="pcp"
BINTRAY_PACKAGE="pcp"
BINTRAY_PARAMS=""

if [ -e "hosts/${CI_HOST}/env.sh" ]; then
    . "hosts/${CI_HOST}/env.sh"
fi
