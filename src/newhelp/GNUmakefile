#
# Copyright (c) 2015 Red Hat.
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

TOPDIR = ../..
include	$(TOPDIR)/src/include/builddefs
include	$(TOPDIR)/src/libpcp/src/GNUlibrarydefs

TARGETS = newhelp$(EXECSUFFIX) chkhelp$(EXECSUFFIX)
CFILES = newhelp.c chkhelp.c

LLDFLAGS = $(WARN_OFF)
LLDLIBS = $(PCP_PMDALIB)

ifneq "$(CROSS_COMPILING)" "yes"
STATICTARGETS = newhelp.static$(EXECSUFFIX)
STATIC_LIBPCP = $(TOPDIR)/src/libpcp_static/src/libpcp_static.a
STATIC_LDLIBS = $(PCPLIB_EXTRAS) $(LIBPCP_LDLIBS) $(LIB_FOR_BACKTRACE)
endif

LDIRT = $(TARGETS) $(STATICTARGETS)

.NOTPARALLEL:

default:	$(TARGETS) $(STATICTARGETS)

install:	default
	$(INSTALL) -m 755 $(TARGETS) $(PCP_BINADM_DIR)

include $(BUILDRULES)

newhelp$(EXECSUFFIX):	newhelp.c
	$(CCF) -o $@ $(LDFLAGS) newhelp.c $(LDLIBS)

newhelp.static$(EXECSUFFIX):	newhelp.c $(STATIC_LIBPCP)
	$(CCF) -DPCP_STATIC -o $@ $(LDFLAGS) newhelp.c $(STATIC_LIBPCP) $(STATIC_LDLIBS)

chkhelp$(EXECSUFFIX):	chkhelp.o
	$(CCF) -o $@ $(LDFLAGS) chkhelp.o $(LDLIBS)

default_pcp:	default

install_pcp:	install

chkhelp.o newhelp.o:	$(TOPDIR)/src/include/pcp/libpcp.h

check:: $(CFILES)
	$(CLINT) $^
