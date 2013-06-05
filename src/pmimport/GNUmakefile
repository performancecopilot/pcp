#
# Copyright (c) 1995-2002 Silicon Graphics, Inc.  All Rights Reserved.
# Copyright (c) 2009 Aconex.  All Rights Reserved.
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

TOPDIR = ../..
include $(TOPDIR)/src/include/builddefs

SUBDIRS = sar2pcp sheet2pcp iostat2pcp mrtg2pcp collectl2pcp

default: $(SUBDIRS)
	$(SUBDIRS_MAKERULE)

include $(BUILDRULES)

install: default $(SUBDIRS)
	$(SUBDIRS_MAKERULE)

default_pcp: default

install_pcp: install
