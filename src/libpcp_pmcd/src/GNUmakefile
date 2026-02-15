#
# Copyright (c) 2013-2015,2022,2025 Red Hat.
# Copyright (c) 2008 Aconex.  All Rights Reserved.
# Copyright (c) 2000,2003,2004 Silicon Graphics, Inc.  All Rights Reserved.
#
# This library is free software; you can redistribute it and/or modify it
# under the terms of the GNU Lesser General Public License as published
# by the Free Software Foundation; either version 2.1 of the License, or
# (at your option) any later version.
#
# This library is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
# or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
# License for more details.
#

TOPDIR = ../../..
include $(TOPDIR)/src/include/builddefs

CFILES = data.c trace.c client.c
SCRIPT = pmcd.bt
# made from pmcd.bt.in by configure
LDIRT	+= pmcd.bt

LCFLAGS = -I$(TOPDIR)/src/pmcd/src -I$(TOPDIR)/src/libpcp/src -DPMCD_INTERNAL
LLDLIBS = $(PCPLIB)

ifeq "$(TARGET_OS)" "mingw"
LIBTARGET = libpcp_pmcd.$(DSOSUFFIX)
LDIRT	+= libpcp_pmcd.a
else
STATICLIBTARGET = libpcp_pmcd.a
LDIRT	+= libpcp_pmcd.$(DSOSUFFIX)
endif

default : $(LIBTARGET) $(STATICLIBTARGET) $(SCRIPT)

$(SCRIPT) : $(SCRIPT).in
	$(SED) -e 's;@PMCD_PATH@;'$(PCP_BINADM_DIR)/pmcd';' $< > $@

include $(BUILDRULES)

install : default
ifeq "$(TARGET_OS)" "mingw"
	$(INSTALL) -m 755 $(LIBTARGET) $(PCP_LIB_DIR)/$(LIBTARGET)
endif

default_pcp : default

install_pcp : install

client.o:	$(TOPDIR)/src/include/pcp/libpcp.h
