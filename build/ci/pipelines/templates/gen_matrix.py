#!/usr/bin/env python3
import os
import json
import re

hosts = re.split(r"[ ,]", os.environ.get("HOSTS", "all"))
if hosts in ([""], ["all"]):
    hosts = os.listdir("./build/ci/hosts")

print("##vso[task.setVariable variable=matrix;isOutput=true]" + json.dumps({host: {"ci_host": host} for host in hosts}))
