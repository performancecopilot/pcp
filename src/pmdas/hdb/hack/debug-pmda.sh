#!/usr/bin/env bash
set -eu -o pipefail
IMAGE_NAME=pcp-pmda-hdb:latest
SCRIPT_DIR="$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"
PROJECT_ROOT=$SCRIPT_DIR/..
MOUNT_PATH=/pcp-pmda-hdb

# create config file with credentials
printf "[hdb]\nhost=%s\nport=%s\nuser=%s\npassword=%s\n" "$HDB_HOST" "$HDB_PORT" "$HDB_USER" "$HDB_PASSWORD" > "$SCRIPT_DIR"/pmdahdb.conf

echo "Run the following commands once you see the dbpmda> prompt"
echo ""
echo "open pipe ./../pmdahdb.py --conf=./pmdahdb.conf"
echo "getiname on"
echo "getdesc on"
echo "traverse hdb"
echo ""
docker run -it --workdir "$MOUNT_PATH/hack" -v "$PROJECT_ROOT":"$MOUNT_PATH" "$IMAGE_NAME" dbpmda -n pmns
