#
# Copyright (c) 2008-2009 Aconex.  All Rights Reserved.
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

CFILES = pcp-services.c pcp-setevent.c pcp-eventlog.c
TARGETS = pcp-services.exe pcp-setevent.exe pcp-eventlog.exe
WRAPPERS = pcp.bat pmafm.bat mkaf.bat
LLDLIBS = -lpcp
LSRCFILES = $(WRAPPERS)

default:	build-me

include $(BUILDRULES)

ifeq "$(TARGET_OS)" "mingw"
build-me:	$(TARGETS)
install:	default
	$(INSTALL) -m 755 $(TARGETS) $(PCP_BIN_DIR)
	$(INSTALL) -m 755 $(WRAPPERS) $(PCP_BIN_DIR)
else
build-me:
install:
endif

pcp-eventlog.exe:	pcp-eventlog.o
	$(CCF) -o $@ $(LDFLAGS) pcp-eventlog.o $(LDLIBS)

pcp-setevent.exe:	pcp-setevent.o
	$(CCF) -o $@ $(LDFLAGS) pcp-setevent.o $(LDLIBS)

pcp-services.exe:	pcp-services.o
	$(CCF) -o $@ $(LDFLAGS) pcp-services.o $(LDLIBS)

default_pcp:	default

install_pcp:	install
