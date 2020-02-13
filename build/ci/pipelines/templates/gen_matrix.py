#!/usr/bin/env python3
import sys
import os
import json
import re

all_hosts = sorted(os.listdir("./build/ci/hosts"))
if len(sys.argv) > 1 and sys.argv[1] not in ["", "all"]:
    hosts = re.split(r"[ ,]", sys.argv[1])
    for host in hosts:
        if host not in all_hosts:
            raise Exception(f"{host} is not supported, supported hosts: {', '.join(all_hosts)}")
else:
    hosts = all_hosts

print("##vso[task.setVariable variable=matrix;isOutput=true]" + json.dumps({host: {"ci_host": host} for host in hosts}))
