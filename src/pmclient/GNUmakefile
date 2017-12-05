#
# Copyright (c) 2013-2016 Red Hat.
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

CFILES	= pmclient.c pmclient_fg.c
PYFILES = pmclient_fg.python
CMDTARGET1 = pmclient$(EXECSUFFIX)
CMDTARGET2 = pmclient_fg$(EXECSUFFIX)
TARGETS = $(CMDTARGET1) $(CMDTARGET2)
LLDLIBS	= $(PCPLIB)
LDIRT	= pmnsmap.h mylog.* runme.sh *.o $(TARGETS)
CONFIGS	= pmnsmap.spec pmlogger.config
LSRCFILES = README GNUmakefile.install $(CONFIGS)
DEMODIR	= $(PCP_DEMOS_DIR)/pmclient

default:	$(TARGETS)

include $(BUILDRULES)

install:	$(TARGETS)
	$(INSTALL) -m 755 $(CMDTARGET1) $(PCP_BIN_DIR)/$(CMDTARGET1)
	$(INSTALL) -m 755 $(CMDTARGET2) $(PCP_BIN_DIR)/$(CMDTARGET2)
	$(INSTALL) -m 755 -d $(DEMODIR)
	$(INSTALL) -m 644 GNUmakefile.install $(DEMODIR)/Makefile
	$(INSTALL) -m 644 $(CFILES) $(CONFIGS) README $(DEMODIR)
	$(INSTALL) -m 755 $(PYFILES) $(DEMODIR)/$(PYFILES)

pmclient.o:	pmnsmap.h

pmclient$(EXECSUFFIX):  pmclient.o 
	rm -f $@
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS) $(LDLIBS)

pmclient_fg$(EXECSUFFIX):  pmclient_fg.o
	rm -f $@
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS) $(LDLIBS)

pmgenmap.sh:
	sed -e "s;^\. .PCP_DIR.etc.pcp.env;. $(TOPDIR)/src/include/pcp.env;" \
	$(TOPDIR)/src/pmgenmap/pmgenmap.sh > pmgenmap.sh

pmnsmap.h:	pmgenmap.sh pmnsmap.spec
	$(PMGENMAP) pmnsmap.spec > pmnsmap.h

default_pcp:	default

install_pcp:	install

pmclient.o pmclient_fg.o:	$(TOPDIR)/src/include/pcp/libpcp.h
