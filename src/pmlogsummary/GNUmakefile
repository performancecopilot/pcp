#
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

CFILES	= pmlogsummary.c
CMDTARGET = pmlogsummary$(EXECSUFFIX)
LLDLIBS	= $(PCPLIB) $(LIB_FOR_MATH)

default:	$(CMDTARGET)

include $(BUILDRULES)

install:	$(CMDTARGET)
	$(INSTALL) -m 755 $(CMDTARGET) $(PCP_BIN_DIR)/$(CMDTARGET)
	$(INSTALL) -m 755 pmdiff.sh $(PCP_BIN_DIR)/pmdiff
	$(INSTALL) -S $(PCP_BIN_DIR)/pmdiff $(PCP_BINADM_DIR)/pmwtf

default_pcp:	default

install_pcp:	install
