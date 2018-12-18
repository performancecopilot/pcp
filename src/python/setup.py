""" Build script for the PCP python package """
#
# Copyright (C) 2012-2018 Red Hat.
# Copyright (C) 2009-2012 Michael T. Werner
#
# This file is part of the "pcp" module, the python interfaces for the
# Performance Co-Pilot toolkit.
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

# New way, modern setup mechanisms for pypi
from setuptools import setup, find_packages, Extension
# To use a consistent encoding
from codecs import open
from os import path

# Get the long description from the README file
here = path.abspath(path.dirname(__file__))
with open(path.join(here, 'README.rst'), encoding='utf-8') as f:
    long_description = f.read()

setup(name = 'pcp',
    version = '4.1',
    description = 'Performance Co-Pilot collector, monitor and instrumentation APIs',
    long_description = long_description,
    license = 'GPLv2+',
    author = 'Performance Co-Pilot Development Team',
    author_email = 'pcp@groups.io',
    url = 'https://pcp.io',
    packages = find_packages(),
    ext_modules = [
        Extension('cpmapi', ['pmapi.c'], libraries = ['pcp']),
        Extension('cpmda', ['pmda.c'], libraries = ['pcp_pmda', 'pcp']),
        Extension('cpmgui', ['pmgui.c'], libraries = ['pcp_gui', 'pcp']),
        Extension('cpmi', ['pmi.c'], libraries = ['pcp_import', 'pcp']),
        Extension('cmmv', ['mmv.c'], libraries = ['pcp_mmv', 'pcp']),
    ],
    keywords = ['performance', 'analysis', 'monitoring' ],
    platforms = [ 'Windows', 'Linux', 'FreeBSD', 'NetBSD', 'OpenBSD', 'Solaris', 'Mac OS X', 'AIX' ],
    classifiers = [
        'Development Status :: 5 - Production/Stable',
        'Intended Audience :: Developers',
        'Intended Audience :: System Administrators',
        'Intended Audience :: Information Technology',
        'License :: OSI Approved :: GNU General Public License (GPL)',
        'Natural Language :: English',
        'Operating System :: MacOS :: MacOS X',
        'Operating System :: Microsoft :: Windows',
        'Operating System :: POSIX',
        'Operating System :: POSIX :: AIX',
        'Operating System :: POSIX :: Linux',
        'Operating System :: POSIX :: BSD :: NetBSD',
        'Operating System :: POSIX :: BSD :: OpenBSD',
        'Operating System :: POSIX :: BSD :: FreeBSD',
        'Operating System :: POSIX :: SunOS/Solaris',
        'Operating System :: Unix',
        'Topic :: System :: Logging',
        'Topic :: System :: Monitoring',
        'Topic :: System :: Networking :: Monitoring',
        'Topic :: Software Development :: Libraries',
    ],
)
