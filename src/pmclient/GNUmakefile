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

CFILES	= pmclient.c
CMDTARGET = pmclient$(EXECSUFFIX)
LLDLIBS	= $(PCPLIB)
LDIRT	= pmnsmap.h mylog.* runme.sh
LSRCFILES = README GNUmakefile.install pmlogger.config pmnsmap.spec

PMLOGGERCONFIG=$(PCP_SYSCONF_DIR)/pmlogger
DEMODIR=$(PCP_DEMOS_DIR)/pmclient

default:	$(CMDTARGET)

include $(BUILDRULES)

install:	$(CMDTARGET)
	$(INSTALL) -m 755 $(CMDTARGET) $(PCP_BIN_DIR)/$(CMDTARGET)
	$(INSTALL) -m 644 pmlogger.config $(PMLOGGERCONFIG)/config.pmclient
	$(INSTALL) -m 755 -d $(DEMODIR)
	$(INSTALL) -m 644 GNUmakefile.install $(DEMODIR)/Makefile
	$(INSTALL) -m 644 $(CFILES) README pmnsmap.spec $(DEMODIR)

pmclient.o:	pmnsmap.h

pmnsmap.h:	pmnsmap.spec
	sed -e "s;^\. .PCP_DIR.etc.pcp.env;. $(TOPDIR)/src/include/pcp.env;" \
	../pmgenmap/pmgenmap.sh >runme.sh; \
	$(RUN_IN_BUILD_ENV) sh ./runme.sh pmnsmap.spec >pmnsmap.h

default_pcp:	default

install_pcp:	install
