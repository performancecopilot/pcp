#
# Copyright (c) 2000,2004 Silicon Graphics, Inc.  All Rights Reserved.
# Copyright (c) 2009,2011 Aconex.  All Rights Reserved.
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
-include GNUlocaldefs

CFILES	= callback.c open.c mainloop.c help.c cache.c tree.c context.c \
	  events.c queues.c
HFILES	= libdefs.h queues.h
LLDLIBS	= -lpcp

STATICLIBTARGET = libpcp_pmda.a

#
# libpcp_pmda.so   -> libpcp_pmda.so.3
# libpcp_pmda.so.2 -> libpcp_pmda.so.3
#
DSOVERSION_V2 = 2
DSOVERSION_V3 = 3

ifneq "$(TARGET_OS)" "darwin"
LIBTARGET_V1 = libpcp_pmda.$(DSOSUFFIX)
LIBTARGET_V2 = libpcp_pmda.$(DSOSUFFIX).$(DSOVERSION_V2)
LIBTARGET_V3 = libpcp_pmda.$(DSOSUFFIX).$(DSOVERSION_V3)
else
LIBTARGET_V1 = libpcp_pmda.$(DSOSUFFIX)
LIBTARGET_V2 = 
LIBTARGET_V3 = libpcp_pmda.$(DSOVERSION_V3).$(DSOSUFFIX)
endif
ifeq "$(PACKAGE_DISTRIBUTION)" "debian"
LIBTARGET_V2 = 
endif
LIBTARGET = $(LIBTARGET_V3)

ifeq "$(TARGET_OS)" "mingw"
LIBTARGET = libpcp_pmda.$(DSOSUFFIX)
STATICLIBTARGET =
LIBTARGET_V1 =
LIBTARGET_V2 =
LIBTARGET_V3 =
endif

ifeq "$(ENABLE_SHARED)" "no"
LIBTARGET =
LIBTARGET_V1 =
LIBTARGET_V2 =
LIBTARGET_V3 =
endif

LDIRT = $(LIBTARGET_V1) $(LIBTARGET_V2) $(LIBTARGET_V3) links.done

default: $(LIBTARGET_V1) $(LIBTARGET_V2) $(LIBTARGET) $(STATICLIBTARGET) links.done

ifneq "$(LIBTARGET_V1)" ""
$(LIBTARGET_V1): $(LIBTARGET_V3)
	$(LN_S) -f $(LIBTARGET_V3) $(LIBTARGET_V1)
endif
ifneq "$(LIBTARGET_V2)" ""
$(LIBTARGET_V2): $(LIBTARGET_V3)
	$(LN_S) -f $(LIBTARGET_V3) $(LIBTARGET_V2)
endif

callback.o mainloop.o open.o : libdefs.h

include $(BUILDRULES)

install : default
ifneq "$(LIBTARGET)" ""
	$(INSTALL) -m 755 $(LIBTARGET) $(PCP_LIB_DIR)/$(LIBTARGET)
endif
ifneq "$(LIBTARGET_V1)" ""
	$(INSTALL) -S $(LIBTARGET_V3) $(PCP_LIB_DIR)/$(LIBTARGET_V1)
endif
ifneq "$(LIBTARGET_V2)" ""
	$(INSTALL) -S $(LIBTARGET) $(PCP_LIB_DIR)/$(LIBTARGET_V2)
endif
ifneq "$(STATICLIBTARGET)" ""
	$(INSTALL) -m 755 $(STATICLIBTARGET) $(PCP_LIB_DIR)/$(STATICLIBTARGET)
endif

default_pcp : default

install_pcp : install

# Populate a common lib directory for simplifying the builds of some other
# components that need to find multiple PCP libraries easily.  Also makes
# use of LD_LIBRARY_PATH for debugging exercises a little simpler.
# 
links.done: $(STATICLIBTARGET) $(LIBTARGET)
ifneq "$(LIBTARGET)" ""
	$(LN_S) -f $(shell pwd)/libpcp_pmda.$(DSOSUFFIX) $(TOPDIR)/src/lib
endif
ifneq "$(STATICLIBTARGET)" ""
	$(LN_S) -f $(shell pwd)/$(STATICLIBTARGET) $(TOPDIR)/src/lib
endif
