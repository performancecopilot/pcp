""" Build script for the PCP python package """
#
# Copyright (C) 2012-2014 Red Hat.
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

from distutils.core import setup, Extension

setup(name = 'pcp',
    version = '1.0',
    description = 'Python package for Performance Co-Pilot',
    license = 'GPLv2+',
    author = 'Performance Co-Pilot Development Team',
    author_email = 'pcp@oss.sgi.com',
    url = 'http://www.pcp.io',
    packages = ['pcp'],
    ext_modules = [
        Extension('cpmapi', ['pmapi.c'], libraries = ['pcp']),
        Extension('cpmda', ['pmda.c'], libraries = ['pcp_pmda', 'pcp']),
        Extension('cpmgui', ['pmgui.c'], libraries = ['pcp_gui']),
        Extension('cpmi', ['pmi.c'], libraries = ['pcp_import']),
        Extension('cmmv', ['mmv.c'], libraries = ['pcp_mmv']),
    ],
    platforms = [ 'Windows', 'Linux', 'FreeBSD', 'NetBSD', 'Solaris', 'Mac OS X', 'AIX' ],
    long_description =
        'PCP provides services to support system-level performance monitoring',
    classifiers = [
        'Development Status :: 5 - Production/Stable'
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
        'Operating System :: POSIX :: BSD :: FreeBSD',
        'Operating System :: POSIX :: SunOS/Solaris',
        'Operating System :: Unix',
        'Topic :: System :: Logging',
        'Topic :: System :: Monitoring',
        'Topic :: System :: Networking :: Monitoring',
        'Topic :: Software Development :: Libraries',
    ],
)
