# 
# Copyright (c) 2013,2017,2020 Red Hat.
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
include $(TOPDIR)/src/libpcp/src/GNUlibrarydefs

MKFILE_SUBDIRS = cpu entropy filesys memory network percpu perdisk pernetif \
		 power global primary zeroconf
SUBDIRS	= $(MKFILE_SUBDIRS)

CMDTARGET = pmieconf$(EXECSUFFIX)
CFILES = pmieconf.c rules.c io.c
HFILES = rules.h

PMLOGCONF_ETC_TOOLS = $(PCP_SYSCONF_DIR)/pmlogconf/tools
PMLOGCONF_VAR_TOOLS = $(PCP_VAR_DIR)/config/pmlogconf/tools

LLDLIBS = $(PCPLIB)
LCFLAGS = -I$(TOPDIR)/src/pmie/src

ifneq "$(CROSS_COMPILING)" "yes"
STATICTARGETS = pmieconf.static$(EXECSUFFIX)
STATIC_LIBPCP = $(TOPDIR)/src/libpcp_static/src/libpcp_static.a
STATIC_LDLIBS = $(PCPLIB_EXTRAS) $(LIBPCP_LDLIBS) $(LIB_FOR_BACKTRACE)
endif

LDIRT = local $(CMDTARGET) $(STATICTARGETS) rate-syscalls \
	pmlogconf.tmp pmlogconf $(shell echo */GNUmakefile)
LDIRDIRT = rules .pcp

.NOTPARALLEL:

default: $(CMDTARGET) $(STATICTARGETS) makefiles local pmlogconf

# for src-link-pcp target from buildrules
$(SUBDIRS):	makefiles

$(CMDTARGET): $(OBJECTS)

pmieconf.o rules.o:    rules.h

makefiles:
	@for d in $(MKFILE_SUBDIRS); do \
	    rm -f $$d/GNUmakefile; \
	    cd $$d; \
	    $(LN_S) ../GNUmakefile.rules GNUmakefile; \
	    cd ..; \
	done

pmieconf.static$(EXECSUFFIX): $(CFILES) $(STATIC_LIBPCP)
	$(CCF) -DPCP_STATIC -o $@ $(LDFLAGS) $(CFILES) $(STATIC_LIBPCP) $(STATIC_LDLIBS)

local: $(STATICTARGETS) $(SUBDIRS)
	@rm -fr rules; mkdir rules
	$(SUBDIRS_MAKERULE)
	HOME=`pwd` $(PMIECONF) -F -r rules -f local
	HOME=`pwd` $(PMIECONF) -r rules -f local modify primary enabled yes

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
	$(INSTALL) -m 644 -t $(PMLOGCONF_VAR_TOOLS)/pmieconf pmlogconf $(PMLOGCONF_ETC_TOOLS)/pmieconf

include $(BUILDRULES)

default_pcp: default

install_pcp: install

io.o rules.o:	$(TOPDIR)/src/include/pcp/libpcp.h
