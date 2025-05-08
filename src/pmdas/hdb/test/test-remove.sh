#!/usr/bin/env bash
set -eux -o pipefail

# install preparation
SCRIPT_DIR="$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"
PROJECT_MOUNT=$SCRIPT_DIR/..
INSTALL_DIR=/var/lib/pcp/pmdas/hdb
mkdir -p "$INSTALL_DIR"
cp  "$PROJECT_MOUNT/pmdahdb.py" "$INSTALL_DIR/pmdahdb.python"
cp  "$PROJECT_MOUNT/Install" "$INSTALL_DIR"
cp  "$PROJECT_MOUNT/Remove" "$INSTALL_DIR"
cd "$INSTALL_DIR"

# actual installation
./Install
# pmda is marked as installed
pcp | grep -e 'pmda:.* hdb.*'
# remove
./Remove 
# pmda is removed 
pcp | (! grep -e 'pmda:.* hdb.*')