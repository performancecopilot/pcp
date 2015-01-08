#
# Copyright (c) 2013-2014 Red Hat.
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
-include ./GNUlocaldefs

CFILES = data.c trace.c client.c

LCFLAGS = -I$(TOPDIR)/src/pmcd/src -I$(TOPDIR)/src/libpcp/src -DPMCD_INTERNAL
LLDLIBS = -lpcp

ifeq "$(ENABLE_SECURE)" "true"
LCFLAGS += $(NSSCFLAGS) $(NSPRCFLAGS)
endif

ifeq "$(ENABLE_PROBES)" "true"
ifeq "$(TARGET_OS)" "linux"
OBJECTS += probes.o
endif
LDIRT += probes.h pmcd.stp
endif

ifeq "$(TARGET_OS)" "mingw"
LIBTARGET = libpcp_pmcd.$(DSOSUFFIX)
LDIRT	+= libpcp_pmcd.a
else
STATICLIBTARGET = libpcp_pmcd.a
LDIRT	+= libpcp_pmcd.$(DSOSUFFIX)
endif

default : $(LIBTARGET) $(STATICLIBTARGET)

# Static probing for Linux, Mac OS X and Solaris.
ifeq "$(ENABLE_PROBES)" "true"
ifneq "$(findstring $(TARGET_OS),linux darwin freebsd)" ""
trace.o: probes.h
probes.h: probes.d pmcd.stp
	$(DTRACE) -h -s $< -o $@
probes.o: probes.d
	$(DTRACE) -G -s $< -o $@
endif
ifeq "$(TARGET_OS)" "solaris"
$(STATICLIBTARGET): rewrite
trace.o: probes.h
probes.h: probes.d
	$(DTRACE) -h -s $< -o $@
rewrite: trace.o
	$(DTRACE) -G -s probes.d trace.o
	touch rewrite
LDIRT += rewrite
endif
endif

pmcd.stp : pmcd.stp.in
	$(SED) -e 's;@path@;'$(PCP_BINADM_DIR)/pmcd';' $< > $@

include $(BUILDRULES)

install : default
ifeq "$(TARGET_OS)" "mingw"
	$(INSTALL) -m 755 $(LIBTARGET) $(PCP_LIB_DIR)/$(LIBTARGET)
endif

ifeq "$(ENABLE_PROBES)" "true"
ifeq "$(TARGET_OS)" "linux"
	$(INSTALL) -m 755 -d $(PCP_SHARE_DIR)/../systemtap/tapset
	$(INSTALL) -m 444 pmcd.stp $(PCP_SHARE_DIR)/../systemtap/tapset/pmcd.stp
endif
endif

default_pcp : default

install_pcp : install
