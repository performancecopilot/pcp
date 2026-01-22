#!/bin/bash
# Run integration tests
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
exec "$SCRIPT_DIR/integration/run-integration-tests.sh" "$@"
