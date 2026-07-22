#
# Copyright (c) 2013-2015 Red Hat.
# Copyright (c) 2001,2009 Silicon Graphics, Inc.  All Rights Reserved.
# Copyright (c) 2022 Ken McDonell.  All Rights Reserved.
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

CFILES	= io.c indom.c
HFILES	= 

LIBCONFIG = libpcp_archive.pc
VERSION_SCRIPT = exports

STATICLIBTARGET	= libpcp_archive.a
DSOVERSION = 1
LIBTARGET = libpcp_archive.$(DSOSUFFIX).$(DSOVERSION)
SYMTARGET = libpcp_archive.$(DSOSUFFIX) 
ifeq "$(TARGET_OS)" "darwin"
LIBTARGET = libpcp_archive.$(DSOVERSION).$(DSOSUFFIX)
endif
ifeq "$(TARGET_OS)" "mingw"
LIBTARGET = libpcp_archive.$(DSOSUFFIX) 
SYMTARGET =
STATICLIBTARGET	=
endif
ifeq "$(ENABLE_SHARED)" "no"
LIBTARGET =
SYMTARGET =
endif

LCFLAGS = -DPCP_INTERNAL
LLDLIBS = -lpcp
LDIRT = $(SYMTARGET) $(LIBCONFIG)

default:  $(LIBTARGET) $(SYMTARGET) $(STATICLIBTARGET) $(LIBCONFIG)

$(OBJECTS): $(HFILES)

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

default_pcp: default

install_pcp: install

ifneq ($(SYMTARGET),)
$(SYMTARGET):
	$(LN_S) -f $(LIBTARGET) $@
endif

ifneq ($(LIBTARGET),)
$(LIBTARGET): $(VERSION_SCRIPT)
$(LIBCONFIG): $(LIBCONFIG).in
	@sed -e's|@PREFIX@|$(PCP_PREFIX)|g' \
	     -e's|@LIBDIR@|$(PCP_LIB_DIR)|g' \
	     -e's|@INCDIR@|$(PCP_INC_DIR)|g' \
	     -e's|@VERSION@|$(PCP_VERSION)|g' <$? >$@
endif

$(OBJECTS):	$(TOPDIR)/src/include/pcp/archive.h
