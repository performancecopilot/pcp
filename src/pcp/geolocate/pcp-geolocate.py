#!/usr/bin/pmpython
#
# Copyright (C) 2022-2023 Red Hat.
#
# This program is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by the
# Free Software Foundation; either version 2 of the License, or (at your
# option) any later version.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
# or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
# for more details.
#
# pylint: disable=broad-except, bare-except, protected-access
""" Display geographical location from local cache or IP lookup """

from pcp.pmapi import pmContext as PCP
try:
    import urllib.request as httprequest
except Exception:
    import urllib2 as httprequest
import threading
import time
import json
import sys
import os

start = time.time()
output = '{"latitude":%s,"longitude":%s}'
writing = False
stdoutfd = sys.stdout

# first extract from cached (labels) file
try:
    if len(sys.argv) == 2:  # use single argument if presented
        path = sys.argv[1]
    else:
        path = PCP.pmGetConfig('PCP_SYSCONF_DIR')
        path += '/labels/optional/geolocate'
    cached = json.load(path)
    print(output % (cached['latitude'], cached['longitude']))
except:
    try:
        sys.stdout = open(path, 'w')    # create (or overwrite)
        writing = True
    except PermissionError:
        sys.stderr.write('No permission to write to %s\n' % path)
        sys.stdout = stdoutfd


# setup threads for handling parallel REST API requests
threads = []

def finish(url):
    if writing:
        sys.stdout.close()
        sys.stdout = stdoutfd
    #print("'%s\' fetched in %ss" % (url, time.time() - start))
    os._exit(0) # not sys.exit, which awaits all threads


def ipinfo(url):
    with httprequest.urlopen(url) as http:
        js = http.read().decode('utf-8')
        data = json.loads(js)
        coords = data['loc'].split(',')
        print(output % (coords[0], coords[1]))
    finish(url)

IPINFO = "https://ipinfo.io/json"
threads.append(threading.Thread(target = ipinfo, args = (IPINFO, )))


def mozilla(url):
    with httprequest.urlopen(url) as http:
        js = http.read().decode('utf-8')
        data = json.loads(js)
        coords = data['location']
        print(output % (coords['lat'], coords['lng']))
    finish(url)

MOZILLA = "https://location.services.mozilla.com/v1/geolocate?key=geoclue"
threads.append(threading.Thread(target = mozilla, args = (MOZILLA,)))


try:
    for thread in threads:
        thread.start()
        thread.join()
except KeyboardInterrupt:
    finish('(none)')
