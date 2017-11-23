#
# Copyright (c) 2017 Red Hat.
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

CFILES	= pmseries.c
CMDTARGET = pmseries$(EXECSUFFIX)
LLDLIBS	= $(PCP_WEBLIB) $(LIB_FOR_HIREDIS) $(LIB_FOR_MATH)
LLDFLAGS = -L$(TOPDIR)/src/libpcp_web/src

default:	$(CMDTARGET)

install:	default
	#$(INSTALL) -m 755 $(CMDTARGET) $(PCP_BIN_DIR)/$(CMDTARGET)

include $(BUILDRULES)

default_pcp : default

install_pcp : install
