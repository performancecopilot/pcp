#!/bin/bash -e

cd "$(dirname "$0")"

export RESOURCE_GROUP="PCP_Builds"
export DISTRIBUTION="$2"
export IMAGE="agent-image-${DISTRIBUTION}"
export VMSS="build-${BUILD_ID}-${DISTRIBUTION}"

script=$1
lookup_directories=$(grep -P "^${DISTRIBUTION}( |$)" distributions/supported_distributions)

for distribution in ${lookup_directories} default
do
    distribution_path="./distributions/${distribution}"
    script_path="${distribution_path}/${script}"
    if [ -f "${script_path}" ]; then
        echo "Running ${script_path}..."
        cd "${distribution_path}"
        exec "./${script}" "${@:3}"
    fi
done

echo "Script not found."
exit 1
