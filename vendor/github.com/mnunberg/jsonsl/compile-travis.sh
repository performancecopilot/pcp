#!/bin/bash
set -ev

if [ "${PARSE_NAN}" = "1" ]; then
  make JSONSL_PARSE_NAN=1 && make JSONSL_PARSE_NAN=1 check
else
  make && make check
fi
