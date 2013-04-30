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
include	$(TOPDIR)/src/include/builddefs

MKFILE_SUBDIRS = cpu filesys memory percpu pernetif global

SUBDIRS	= $(MKFILE_SUBDIRS)

CMDTARGET = pmieconf$(EXECSUFFIX)
CFILES = pmieconf.c rules.c io.c
HFILES = rules.h

LSRCFILES = GNUmakefile.rules check-rules pmie_email xtractnames $(RFILES)

LLDLIBS = $(PCPLIB)
LDIRT = rules local $(CMDTARGET) rate-syscalls \
	pmlogger.tmp pmlogger.config \
	cpu/GNUmakefile filesys/GNUmakefile memory/GNUmakefile \
	percpu/GNUmakefile pernetif/GNUmakefile
LDIRDIRT = .pcp

default_pcp: $(CMDTARGET) makefiles rules pmlogger.config

# for src-link-pcp target from buildrules
$(SUBDIRS):	makefiles

$(CMDTARGET) : $(OBJECTS)

pmieconf.o rules.o:    rules.h

.NOTPARALLEL:
makefiles:
	@for d in $(MKFILE_SUBDIRS); do \
	    rm -f $$d/GNUmakefile; \
	    cd $$d; \
	    $(LN_S) ../GNUmakefile.rules GNUmakefile; \
	    cd ..; \
	done

rules: $(SUBDIRS)
	rm -fr rules local; mkdir rules; exit 0
	$(SUBDIRS_MAKERULE)
	$(RUN_IN_BUILD_ENV) ./$(CMDTARGET) -F -r rules -f local

pmlogger.config: $(SUBDIRS)
	@rm -f pmlogger.config
	@echo "# extracted from pmieconf rules in source tree" >pmlogger.config
	@echo >>pmlogger.config
	@echo "log advisory on default {" >>pmlogger.config
	$(SUBDIRS_MAKERULE) | grep -v '===' >pmlogger.tmp
	@$(PCP_SORT_PROG) -u pmlogger.tmp | sed -e 's/^/    /' >>pmlogger.config
	@echo "}" >>pmlogger.config

install_pcp: install

install: default_pcp $(SUBDIRS)
	$(SUBDIRS_MAKERULE)
	$(INSTALL) -m 755 $(CMDTARGET) $(PCP_BIN_DIR)/$(CMDTARGET)
	$(INSTALL) -m 755 pmie_email $(PCP_BINADM_DIR)/pmie_email
	$(INSTALL) -m 644 pmlogger.config $(PCP_SYSCONF_DIR)/pmlogger/config.pmieconf

include $(BUILDRULES)
