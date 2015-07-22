#
# Copyright (c) 2013-2015 Red Hat.
# Copyright (c) 2009,2011 Aconex.  All Rights Reserved.
# Copyright (c) 2000,2004 Silicon Graphics, Inc.  All Rights Reserved.
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

CFILES	= callback.c open.c mainloop.c help.c cache.c tree.c context.c \
	  events.c queues.c dynamic.c pduroot.c root.c
HFILES	= libdefs.h queues.h
LLDLIBS	= -lpcp
LCFLAGS += -DPMDA_INTERNAL

STATICLIBTARGET = libpcp_pmda.a

DSOVERSION = 3
LIBTARGET = libpcp_pmda.$(DSOSUFFIX).$(DSOVERSION)
SYMTARGET = libpcp_pmda.$(DSOSUFFIX)

VERSION_SCRIPT = exports

ifeq "$(TARGET_OS)" "darwin"
LIBTARGET = libpcp_pmda.$(DSOVERSION).$(DSOSUFFIX)
endif
ifeq "$(TARGET_OS)" "mingw"
LIBTARGET = libpcp_pmda.$(DSOSUFFIX)
STATICLIBTARGET =
SYMTARGET =
endif

ifeq "$(ENABLE_SHARED)" "no"
LIBTARGET =
SYMTARGET =
endif

LDIRT = $(LIBTARGET) $(SYMTARGET) $(STATICLIBTARGET)
LSRCFILES = $(VERSION_SCRIPT)

default: $(LIBTARGET) $(SYMTARGET) $(STATICLIBTARGET)

ifneq ($(SYMTARGET),)
$(SYMTARGET):
	$(LN_S) -f $(LIBTARGET) $@
endif

callback.o mainloop.o open.o : libdefs.h

include $(BUILDRULES)

install: default
ifneq ($(LIBTARGET),)
	$(INSTALL) -m 755 $(LIBTARGET) $(PCP_LIB_DIR)/$(LIBTARGET)
endif
ifneq ($(SYMTARGET),)
	for tt in $(SYMTARGET); do \
	    $(INSTALL) -S $(LIBTARGET) $(PCP_LIB_DIR)/$$tt || exit 1; \
	done
endif
ifneq ($(STATICLIBTARGET),)
	$(INSTALL) -m 755 $(STATICLIBTARGET) $(PCP_LIB_DIR)/$(STATICLIBTARGET)
endif

default_pcp:	default

install_pcp:	install

ifneq ($(LIBTARGET),)
$(LIBTARGET): $(VERSION_SCRIPT)
endif
