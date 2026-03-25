#!/bin/sh
#
# Install Python build/test dependencies for PCP on macOS.
# Uses uv (https://docs.astral.sh/uv/) to install directly to the
# system Python, bypassing Homebrew's externally-managed-environment
# restriction. No venv needed — this keeps PCP_PYTHON_PROG pointing
# at the system python, avoiding module discovery issues at runtime.
#
# Usage:
#   ./setup-python-env.sh [requirements-file]
#
# Examples:
#   ./setup-python-env.sh                          # build deps (default)
#   ./setup-python-env.sh requirements-test.txt    # test deps
#
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REQS="${1:-$SCRIPT_DIR/requirements-build.txt}"

if ! command -v uv >/dev/null 2>&1; then
    echo "Error: uv is required. Install with: brew install uv" >&2
    exit 1
fi

PYTHON="$(which python3)"
echo "Installing Python dependencies from $REQS to $PYTHON ..."
uv pip install --break-system-packages --python "$PYTHON" -r "$REQS"

echo ""
echo "Done. Verify with: python3 -c 'import requests'"
