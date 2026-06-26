#
# Copyright (c) 2013-2015 Red Hat.
# Copyright (c) 2001,2009 Silicon Graphics, Inc.  All Rights Reserved.
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
-include ./GNUlocaldefs

CFILES	= import.c stuff.c archive.c
HFILES	= private.h

VERSION_SCRIPT = exports

DSOVERSION = 1
LIBTARGET = libpcp_import.$(DSOSUFFIX).$(DSOVERSION)
ifeq "$(TARGET_OS)" "darwin"
LIBTARGET = libpcp_import.$(DSOVERSION).$(DSOSUFFIX)
endif
ifeq "$(ENABLE_SHARED)" "no"
LIBTARGET =
endif
STATICLIBTARGET	=

LCFLAGS = -DPMI_INTERNAL
LLDLIBS = -lpcp
LDIRT = domain.h

DOMAIN = PMI_DOMAIN

default:  $(LIBTARGET)

$(OBJECTS): $(HFILES) domain.h

include $(BUILDRULES)

install: default
ifneq ($(LIBTARGET),)
	$(INSTALL) -m 755 $(LIBTARGET) $(PCP_LIB_DIR)/$(LIBTARGET)
endif

default_pcp: default

install_pcp: install

ifneq ($(LIBTARGET),)
$(LIBTARGET): $(VERSION_SCRIPT)
endif

domain.h: $(TOPDIR)/src/pmns/stdpmid
	$(DOMAIN_MAKERULE)

$(OBJECTS):	$(TOPDIR)/src/include/pcp/libpcp.h
