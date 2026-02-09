#!/bin/bash
# Install macOS build/test dependencies for PCP
# Usage: ./install-deps.sh [--minimal]
#   --minimal: Skip QA-only packages (install only build-required/build-optional)

set -euo pipefail

SKIP_QA_ONLY=false
if [[ "${1:-}" == "--minimal" ]]; then
  SKIP_QA_ONLY=true
  echo "Running in minimal mode - skipping QA-only packages"
fi

echo "=== Installing Homebrew dependencies ==="
brew update --quiet || true
brew install \
  autoconf \
  coreutils \
  gnu-tar \
  libuv \
  pkg-config \
  python3 \
  python-setuptools \
  unixodbc \
  valkey || true

echo ""
echo "=== Installing Perl CPAN modules (build-required and build-optional) ==="
# Ensure cpanminus is installed
brew install cpanminus || brew upgrade cpanminus || true

# Build-required modules (used by PCP export tools and PMDAs)
sudo cpanm --notest \
  JSON \
  Date::Parse \
  Date::Format \
  XML::TokeParser \
  Spreadsheet::WriteExcel \
  Text::CSV_XS

echo ""
echo "=== Installing Python pip packages (build-required and build-optional) ==="
# Build-required and build-optional packages (used by pcp2* tools)
pip3 install --user --break-system-packages \
  lxml \
  openpyxl \
  psycopg2-binary \
  prometheus_client \
  pyarrow \
  pyodbc \
  requests \
  setuptools \
  wheel

if [ "$SKIP_QA_ONLY" = false ]; then
  echo ""
  echo "=== Installing QA-only packages ==="
  sudo cpanm --notest \
    Spreadsheet::XLSX \
    Spreadsheet::Read
  # Note: Pillow (PIL) can be added here if needed for QA image tests
fi

echo ""
echo "✓ Dependencies installed successfully"
