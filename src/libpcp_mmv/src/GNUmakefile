#
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

CFILES	= mmv_stats.c

STATICLIBTARGET	= libpcp_mmv.a
DSOVERSION = 1
LIBTARGET = libpcp_mmv.$(DSOSUFFIX).$(DSOVERSION)
SYMTARGET = libpcp_mmv.$(DSOSUFFIX) 
ifeq "$(TARGET_OS)" "darwin"
LIBTARGET = libpcp_mmv.$(DSOVERSION).$(DSOSUFFIX)
endif
ifeq "$(TARGET_OS)" "mingw"
LIBTARGET = libpcp_mmv.$(DSOSUFFIX) 
SYMTARGET =
STATICLIBTARGET	=
endif
ifeq "$(ENABLE_SHARED)" "no"
LIBTARGET =
SYMTARGET =
endif

LCFLAGS = -I.
LLDLIBS = -lpcp
LDIRT = $(SYMTARGET) links.done

default: $(LIBTARGET) $(SYMTARGET) $(STATICLIBTARGET) links.done

include $(BUILDRULES)

install: default
ifneq "$(LIBTARGET)" ""
	$(INSTALL) -m 755 $(LIBTARGET) $(PCP_LIB_DIR)/$(LIBTARGET)
endif
ifneq "$(SYMTARGET)" ""
	for tt in $(SYMTARGET); do \
		$(INSTALL) -S $(LIBTARGET) $(PCP_LIB_DIR)/$$tt || exit 1; \
	done
endif
ifneq "$(STATICLIBTARGET)" ""
	$(INSTALL) -m 755 $(STATICLIBTARGET) $(PCP_LIB_DIR)/$(STATICLIBTARGET)
endif

default_pcp: default

install_pcp: install

ifneq "$(SYMTARGET)" ""
$(SYMTARGET):
	$(LN_S) -f $(LIBTARGET) $@
endif
#
# Populate a common lib directory for simplifying the builds of some other
# components that need to find multiple PCP libraries easily.  Also makes
# use of LD_LIBRARY_PATH for debugging exercises a little simpler.
# 
links.done: $(STATICLIBTARGET) $(LIBTARGET)
ifneq "$(LIBTARGET)" ""
	$(LN_S) -f $(shell pwd)/libpcp_mmv.$(DSOSUFFIX) $(TOPDIR)/src/lib
endif
ifneq "$(STATICLIBTARGET)" ""
	$(LN_S) -f $(shell pwd)/$(STATICLIBTARGET) $(TOPDIR)/src/lib
endif
	touch links.done
