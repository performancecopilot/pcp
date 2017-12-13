#
# Copyright (c) 2013 Red Hat.
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

HFILES = hash.h
CFILES	= trace.c hash.c pdu.c pdubuf.c p_ack.c p_data.c ftrace.c
VERSION_SCRIPT = exports

LCFLAGS = -DPMTRACE_DEBUG
LLDLIBS = $(LIB_FOR_PTHREADS) -lpcp
LSRCFILES = $(VERSION_SCRIPT)

STATICLIBTARGET = libpcp_trace.a

DSOVERSION = 2
LIBTARGET = libpcp_trace.$(DSOSUFFIX).$(DSOVERSION)
SYMTARGET = libpcp_trace.$(DSOSUFFIX)

ifeq "$(TARGET_OS)" "darwin"
LIBTARGET = libpcp_trace.$(DSOVERSION).$(DSOSUFFIX)
endif
ifeq "$(TARGET_OS)" "mingw"
STATICLIBTARGET =
LIBTARGET = libpcp_trace.$(DSOSUFFIX)
SYMTARGET =
endif
ifeq "$(ENABLE_SHARED)" "no"
LIBTARGET =
SYMTARGET =
endif

LDIRT = $(SYMTARGET)

base default: $(LIBTARGET) $(SYMTARGET) $(STATICLIBTARGET)

ifneq ($(SYMTARGET),)
$(SYMTARGET):
	$(LN_S) -f $(LIBTARGET) $(SYMTARGET)
endif

include $(BUILDRULES)

install : default
ifneq ($(LIBTARGET),)
	$(INSTALL) -m 755 $(LIBTARGET) $(PCP_LIB_DIR)/$(LIBTARGET)
endif
ifneq ($(SYMTARGET),)
	$(INSTALL) -S $(LIBTARGET) $(PCP_LIB_DIR)/$(SYMTARGET)
endif
ifneq ($(STATICLIBTARGET),)
	$(INSTALL) -m 755 $(STATICLIBTARGET) $(PCP_LIB_DIR)/$(STATICLIBTARGET)
endif

default_pcp : default

install_pcp : install

ifneq ($(LIBTARGET),)
$(LIBTARGET): $(VERSION_SCRIPT)
endif

pdu.o trace.o:	$(TOPDIR)/src/include/pcp/libpcp.h
