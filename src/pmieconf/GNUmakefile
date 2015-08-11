# 
# Copyright (c) 2013 Red Hat.
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

MKFILE_SUBDIRS = cpu filesys memory percpu pernetif global

SUBDIRS	= $(MKFILE_SUBDIRS)

CMDTARGET = pmieconf$(EXECSUFFIX)
CFILES = pmieconf.c rules.c io.c
HFILES = rules.h

PMLOGCONF_TOOLS = $(PCP_VAR_DIR)/config/pmlogconf/tools

LSRCFILES = GNUmakefile.rules check-rules pmie_email xtractnames $(RFILES)

LLDLIBS = $(PCPLIB)
LCFLAGS = -I$(TOPDIR)/src/pmie/src
LDIRT = local $(CMDTARGET) rate-syscalls \
	pmlogconf.tmp pmlogconf \
	cpu/GNUmakefile filesys/GNUmakefile memory/GNUmakefile \
	percpu/GNUmakefile pernetif/GNUmakefile
LDIRDIRT = rules .pcp

default: $(CMDTARGET) makefiles local pmlogconf

# for src-link-pcp target from buildrules
$(SUBDIRS):	makefiles

$(CMDTARGET): $(OBJECTS)

pmieconf.o rules.o:    rules.h

.NOTPARALLEL:
makefiles:
	@for d in $(MKFILE_SUBDIRS); do \
	    rm -f $$d/GNUmakefile; \
	    cd $$d; \
	    $(LN_S) ../GNUmakefile.rules GNUmakefile; \
	    cd ..; \
	done

local: $(SUBDIRS)
	@rm -fr rules; mkdir rules
	$(SUBDIRS_MAKERULE)
	$(PMIECONF) -F -r rules -f local

pmlogconf: $(SUBDIRS)
	@rm -f pmlogconf
	@echo "#pmlogconf-setup 2.0" >pmlogconf
	@echo "ident	metrics used by pmie(1) rules from the pmieconf(1) command" >>pmlogconf
	@echo "force	available" >>pmlogconf
	$(SUBDIRS_MAKERULE) >pmlogconf.tmp
	@grep -v '===' pmlogconf.tmp | $(PCP_SORT_PROG) -u | sed -e 's/^/	/' >>pmlogconf

install: default $(SUBDIRS)
	$(SUBDIRS_MAKERULE)
	$(INSTALL) -m 755 $(CMDTARGET) $(PCP_BIN_DIR)/$(CMDTARGET)
	$(INSTALL) -m 755 pmie_email $(PCP_BINADM_DIR)/pmie_email
	$(INSTALL) -m 644 pmlogconf $(PMLOGCONF_TOOLS)/pmieconf

include $(BUILDRULES)

default_pcp: default

install_pcp: install

