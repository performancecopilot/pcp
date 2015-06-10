#
# Copyright (c) 2014-2015 Red Hat.
# Copyright (c) 2000,2004 Silicon Graphics, Inc.  All Rights Reserved.
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

LSRCFILES = pcp.sh
SUBDIRS = atop free uptime numastat dmcache verify summary iostat shping

default :: default_pcp

default_pcp : $(SUBDIRS)
	$(SUBDIRS_MAKERULE)

install :: install_pcp

install_pcp : $(SUBDIRS)
	$(INSTALL) -m 755 pcp.sh $(PCP_BIN_DIR)/pcp$(SHELLSUFFIX)
	$(SUBDIRS_MAKERULE)

include $(BUILDRULES)
