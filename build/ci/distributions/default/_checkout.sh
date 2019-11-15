#!/bin/sh -xe

cd /home/pcp
rm -rf pcp
git clone --branch "$PCP_COMMIT" --depth 1 https://github.com/performancecopilot/pcp.git
