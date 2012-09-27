#
# setup.py
#
# Copyright (C) 2012 Red Hat.
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

setup(name="pcp",
      version="0.2",
      description="Python Interface to Performance Co-Pilot API",
      author="Michael Werner",
      author_email="mtw@protomagic.com",
      maintainer="Stan Cox",
      maintainer_email="scox@redhat.com",
      url="http://oss.sgi.com/projects/pcp/",
      py_modules = ['pcp','pcpi'],
      ext_modules=[ Extension( "pmapi", ["pmapi.c"],
                               libraries=["pcp"]
                             )
                  ],
      classifiers = [
            'Development Status :: 4 - Beta'
            'Intended Audience :: Developers',
            'Intended Audience :: System Administrators',
            'Intended Audience :: Information Technology',
            'License :: OSI Approved :: GNU Library or Lesser General Public License (LGPL)',
            'Natural Language :: English',
            'Operating System :: MacOS :: MacOS X',
            'Operating System :: Microsoft :: Windows',
            'Operating System :: POSIX',
            'Operating System :: POSIX :: AIX',
            'Operating System :: POSIX :: IRIX',
            'Operating System :: POSIX :: Linux',
            'Operating System :: POSIX :: SunOS/Solaris',
            'Operating System :: Unix',
            'Topic :: System :: Monitoring',
            'Topic :: System :: Networking :: Monitoring',
            'Topic :: Software Development :: Libraries',
      ],
)
