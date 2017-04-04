#
# Copyright (c) 2012,2015 Red Hat.
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

TOPDIR = ../../..
include $(TOPDIR)/src/include/builddefs

CMDTARGET = pmcd$(EXECSUFFIX)
HFILES = client.h pmcd.h
CFILES = pmcd.c config.c dofetch.c dopdus.c dostore.c client.c agent.c util.c

LLDLIBS	= $(PCP_PMDALIB) $(LIB_FOR_DLOPEN) -lpcp_pmcd
PCPLIB_LDFLAGS += -L$(TOPDIR)/src/libpcp_pmcd/$(LIBPCP_ABIDIR)

LLDFLAGS = $(RDYNAMIC_FLAG) $(PIELDFLAGS)
LCFLAGS = $(PIECFLAGS)

default:	$(CMDTARGET)

include $(BUILDRULES)

install:	default
	$(INSTALL) -m 755 $(CMDTARGET) $(PCP_BINADM_DIR)/$(CMDTARGET)

default_pcp:	default

install_pcp:	install
