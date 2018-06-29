#
# Copyright (c) 2015 Red Hat.
# Copyright (c) 2011 Ken McDonell.  All Rights Reserved.
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

TOPDIR = ../..
include $(TOPDIR)/src/include/builddefs
include	$(TOPDIR)/src/libpcp/src/GNUlibrarydefs

CFILES	= pmcpp.c
LLDLIBS = $(PCPLIB)
CMDTARGET = pmcpp$(EXECSUFFIX)

ifneq "$(CROSS_COMPILING)" "yes"
STATICTARGETS = pmcpp.static$(EXECSUFFIX)
ifeq "$(TARGET_OS)" "mingw"
STATIC_LIBPCP = $(TOPDIR)/src/libpcp_static/src/libpcp_static.a
else
STATIC_LIBPCP = $(TOPDIR)/src/libpcp/src/libpcp.a
endif
STATIC_LDLIBS = $(PCPLIB_EXTRAS) $(LIBPCP_LDLIBS)
endif

LDIRT = $(CMDTARGET) $(STATICTARGETS)

default:	$(CMDTARGET) $(STATICTARGETS)

include $(BUILDRULES)

install:	$(CMDTARGET)
	$(INSTALL) -m 755 $(CMDTARGET) $(PCP_BINADM_DIR)/$(CMDTARGET)

default_pcp:	default

install_pcp:	install

pmcpp.static$(EXECSUFFIX):	$(CFILES)
	rm -f $(OBJECTS)
	$(CCF) -DPCP_STATIC -o $@ $(LDFLAGS) $(CFILES) $(STATIC_LIBPCP) $(STATIC_LDLIBS)
