#
# Copyright (c) 2013-2017 Red Hat.
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
-include ./GNUlocaldefs

CFILES	= callback.c open.c mainloop.c help.c cache.c tree.c context.c \
	  events.c queues.c dynamic.c pduroot.c root.c lookup2.c
HFILES	= libdefs.h queues.h
XFILES	= lookup2.c
LLDLIBS	= -lpcp
LCFLAGS += -DPMDA_INTERNAL

LIBCONFIG = libpcp_pmda.pc
VERSION_SCRIPT = exports

STATICLIBTARGET = libpcp_pmda.a

DSOVERSION = 3
LIBTARGET = libpcp_pmda.$(DSOSUFFIX).$(DSOVERSION)
SYMTARGET = libpcp_pmda.$(DSOSUFFIX)

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

LDIRT = $(XFILES) $(SYMTARGET) $(STATICLIBTARGET) $(LIBTARGET) $(LIBCONFIG)

default: $(XFILES) $(SYMTARGET) $(STATICLIBTARGET) $(LIBTARGET) $(LIBCONFIG)

ifneq ($(SYMTARGET),)
$(SYMTARGET):
	$(LN_S) -f $(LIBTARGET) $@
endif

cache.o : lookup2.c
callback.o mainloop.o open.o : libdefs.h

include $(BUILDRULES)

install: default
ifneq ($(LIBTARGET),)
	$(INSTALL) -m 755 $(LIBTARGET) $(PCP_LIB_DIR)/$(LIBTARGET)
	$(INSTALL) -m 644 $(LIBCONFIG) $(PCP_LIB_DIR)/pkgconfig/$(LIBCONFIG)
endif
ifneq ($(SYMTARGET),)
	for tt in $(SYMTARGET); do \
	    $(INSTALL) -S $(LIBTARGET) $(PCP_LIB_DIR)/$$tt || exit 1; \
	done
endif
ifneq ($(STATICLIBTARGET),)
	$(INSTALL) -m 755 $(STATICLIBTARGET) $(PCP_LIB_DIR)/$(STATICLIBTARGET)
endif

$(XFILES):
	$(LN_S) $(TOPDIR)/src/external/$@ .

default_pcp:	default

install_pcp:	install

ifneq ($(LIBTARGET),)
$(LIBTARGET): $(VERSION_SCRIPT)
$(LIBCONFIG): $(LIBCONFIG).in
	@sed -e's|@PREFIX@|$(PCP_PREFIX)|g' \
	     -e's|@LIBDIR@|$(PCP_LIB_DIR)|g' \
	     -e's|@INCDIR@|$(PCP_INC_DIR)|g' \
	     -e's|@VERSION@|$(PCP_VERSION)|g' <$? >$@
endif

$(OBJECTS):	$(TOPDIR)/src/include/pcp/libpcp.h

check::	$(CFILES) $(HFILES) $(XFILES)
	$(CLINT) $^
