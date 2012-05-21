
##############################################################################
#
# setup.py
#
# Copyright 2009, Michael T. Werner
#
# This file is part of pcp, the python extensions for SGI's Performance
# Co-Pilot. Pcp is free software: you can redistribute it and/or modify
# it under the terms of the GNU Lesser General Public License as published
# by the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# Pcp is distributed in the hope that it will be useful, but WITHOUT ANY
# WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
# FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for
# more details. You should have received a copy of the GNU Lesser General
# Public License along with pcp. If not, see <http://www.gnu.org/licenses/>.
#

from distutils.core import setup, Extension

setup(name="pcp",
      version="0.1",
      description="Python Interface to SGI's Performance Co-Pilot client API",
      author="Michael Werner",
      author_email="mtw@protomagic.com",
      url="ftp://oss.sgi.com/www/projects/pcp/download/",
      py_modules = ['pcp','pcpi'],
      ext_modules=[ Extension( "pmapi", ["pmapi.c"],
                               libraries=["pcp"]
                             )
                  ],
      classifiers = [
            'Development Status :: 2 - Pre-Alpha',
            'Intended Audience :: Developers',
            'Intended Audience :: System Administrators',
            'Intended Audience :: Information Technology',
            'License :: OSI Approved :: GNU Library or Lesser General Public License (LGPL)',
            'Natural Language :: English',
            'Operating System :: MacOS :: MacOS X',
            'Operating System :: Microsoft :: Windows',
            'Operating System :: POSIX',
            'Operating System :: POSIX :: AIX',
            'Operating System :: POSIX :: HP-UX',
            'Operating System :: POSIX :: IRIX',
            'Operating System :: POSIX :: Linux',
            'Operating System :: POSIX :: SunOS/Solaris',
            'Operating System :: Unix',
            'Topic :: System :: Monitoring',
            'Topic :: System :: Networking :: Monitoring',
            'Topic :: Software Development :: Libraries',
      ],

)


