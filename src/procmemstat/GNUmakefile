#
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
include $(TOPDIR)/src/include/builddefs

CFILES	= procmemstat.c
TARGETS	=
LLDLIBS	= $(PCPLIB)
LDIRT	= pmnsmap.h mylog.* runme.sh $(TARGETS) pmnsmap.i GNUmakefile.install.xxx
LSRCFILES = README GNUmakefile.install pmnsmap.spec

DEMODIR=$(PCP_DEMOS_DIR)/procmemstat

default: $(TARGETS) GNUmakefile.install.xxx

include $(BUILDRULES)

install: $(TARGETS)
	$(INSTALL) -m 755 -d $(DEMODIR)
	$(INSTALL) -m 644 GNUmakefile.install.xxx $(DEMODIR)/Makefile
	$(INSTALL) -m 644 procmemstat.c README pmnsmap.spec $(DEMODIR)

procmemstat.o : pmnsmap.h

pmgenmap.sh:
	sed -e "s;^\. .PCP_DIR.etc.pcp.env;. $(TOPDIR)/src/include/pcp.env;" \
	$(TOPDIR)/src/pmgenmap/pmgenmap.sh > pmgenmap.sh

pmnsmap.h:	pmgenmap.sh pmnsmap.spec
	$(PMCPP) -D$(TARGET_OS) pmnsmap.spec \
	| sed -e '/^#/d' -e '/^$$/d' >pmnsmap.i
	$(PMGENMAP) pmnsmap.i >pmnsmap.h

GNUmakefile.install.xxx:	GNUmakefile.install
	sed -e "s;<CPP>;$(PCP_BINADM_DIR)/pmcpp -D$(TARGET_OS);" <GNUmakefile.install >GNUmakefile.install.xxx

default_pcp : default

install_pcp : install
