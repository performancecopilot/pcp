#!/usr/bin/env bash
set -eu -o pipefail
IMAGE_NAME=pcp-pmda-hdb:latest
SCRIPT_DIR="$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"
PROJECT_ROOT=$SCRIPT_DIR/..
MOUNT_PATH=/pcp-pmda-hdb

# create config file with credentials
printf "[hdb]\nhost=%s\nport=%s\nuser=%s\npassword=%s\n" "$HDB_HOST" "$HDB_PORT" "$HDB_USER" "$HDB_PASSWORD" > "$SCRIPT_DIR"/pmdahdb.conf

# container for tests
CONTAINER_ID=$(docker run --privileged -d -v /sys/fs/cgroup:/sys/fs/cgroup:ro -v "$PROJECT_ROOT":"$MOUNT_PATH" "$IMAGE_NAME")
printf "\n############## test-install ##############\n"
docker exec "$CONTAINER_ID" "$MOUNT_PATH/test/test-install.sh"

printf "\n############## test-remove ##############\n"
docker exec "$CONTAINER_ID" "$MOUNT_PATH/test/test-remove.sh"
printf "\n##########################################\n"

# cleanup
docker kill "$CONTAINER_ID"