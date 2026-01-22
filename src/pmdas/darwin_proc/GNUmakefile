#
# Copyright (c) 2013-2016,2019-2021,2025 Red Hat.  All Rights Reserved.
# Copyright (c) 2007-2010 Aconex.  All Rights Reserved.
# Copyright (c) 2000-2004 Silicon Graphics, Inc.  All Rights Reserved.
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

TOPDIR = ../../..
include	$(TOPDIR)/src/include/builddefs

IAM		= proc
DOMAIN		= PROC
CMDTARGET	= pmdaproc
LIBTARGET	= pmda_proc.dylib
PMDATMPDIR	= $(PCP_PMDAS_DIR)/$(IAM)
PMDACONFIG	= $(PCP_SYSCONF_DIR)/$(IAM)
PMDAADMDIR	= $(PCP_PMDASADM_DIR)/$(IAM)
CONF_LINE	= "proc	3	pipe	binary		$(PMDATMPDIR)/$(CMDTARGET) -d 3"

CFILES		= kinfo_proc.c contexts.c pmda.c
HFILES		= kinfo_proc.h contexts.h

SCRIPTS		= Install Remove
HELPTARGETS	= help.dir help.pag
LDIRT		= $(HELPTARGETS) domain.h
LLDLIBS		= $(PCP_PMDALIB)

default:	build-me

include $(BUILDRULES)

ifeq "$(TARGET_OS)" "darwin"
build-me: root_proc $(LIBTARGET) $(CMDTARGET) $(HELPTARGETS)
	@if [ -f ../pmcd.conf ]; then \
	    if [ `grep -c $(CONF_LINE) ../pmcd.conf` -eq 0 ]; then \
		echo $(CONF_LINE) >> ../pmcd.conf ; \
	    fi; \
	fi

install: default
	$(INSTALL) -m 755 -d $(PMDAADMDIR)
	$(INSTALL) -m 755 -d $(PMDATMPDIR)
	$(INSTALL) -m 644 -t $(PMDATMPDIR) domain.h help help.dir help.pag root root_proc $(PMDAADMDIR)
	$(INSTALL) -m 755 -d $(PMDACONFIG)
	$(INSTALL) -m 644 access.conf $(PMDACONFIG)/access.conf
	$(INSTALL) -m 755 -t $(PMDATMPDIR) $(LIBTARGET) $(CMDTARGET) $(SCRIPTS) $(PMDAADMDIR)
	$(INSTALL) -m 644 -t $(PCP_PMNS_DIR)/root_proc root_proc $(PCP_PMNSADM_DIR)/root_proc
else
build-me:
install:
endif

.NOTPARALLEL:

default_pcp : default

install_pcp : install

# Unit tests (no installation required)
test: build-me
	@echo "Running Darwin_proc PMDA unit tests..."
	@cd test && ./run-unit-tests.sh

.PHONY: test

$(HELPTARGETS) : help
	$(NEWHELP) -n root_proc -v 2 -o help < help

domain.h: ../../pmns/stdpmid
	$(DOMAIN_MAKERULE)

$(OBJECTS): domain.h

pmda.o:	domain.h
pmda.o kinfo_proc.o:	kinfo_proc.h
contexts.o pmda.o kinfo_proc.o:	$(TOPDIR)/src/include/pcp/libpcp.h
