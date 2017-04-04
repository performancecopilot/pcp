#
# Copyright (c) 2011 Aconex.  All Rights Reserved.
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

TOPDIR = ../../..
include $(TOPDIR)/src/include/builddefs

HFILES = _mingw_unicode.h pshpack8.h \
	evntcons.h evntprov.h evntrace.h \
	pdh.h pdhmsg.h \
	tdh.h tdhmsg.h \
	winevt.h winmeta.h winperf.h wmistr.h

default :: default_pcp

default_pcp : 

include $(BUILDRULES)

install :: default_pcp install_pcp

install_pcp : default_pcp
