#
# Copyright (c) 2000,2003,2004,2008 Silicon Graphics, Inc.  All Rights Reserved.
# Copyright (c) 2007-2010 Aconex.  All Rights Reserved.
# Copyright (c) 2013 Red Hat.
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
LIBTARGET	= pmda_proc.so
PMDAINIT	= proc_init
PMDADIR		= $(PCP_PMDAS_DIR)/$(IAM)
CONF_LINE	= "proc	3	pipe	binary		$(PMDADIR)/$(CMDTARGET) -d 3"

CFILES		= pmda.c cgroups.c filesys.c proc_pid.c proc_runq.c \
		  ksym.c getinfo.c contexts.c

HFILES		= cgroups.h clusters.h proc_pid.h indom.h getinfo.h \
		  proc_runq.h ksym.h filesys.h contexts.h

SCRIPTS		= Install Remove
VERSION_SCRIPT	= exports
HELPTARGETS	= help.dir help.pag
LSRCFILES 	= help root root_proc linux_proc_migrate.conf $(SCRIPTS)
LDIRT		= $(HELPTARGETS) domain.h $(VERSION_SCRIPT)

LLDLIBS		= $(PCP_PMDALIB)
LCFLAGS		= $(INVISIBILITY)

# Uncomment these flags for profiling
# LCFLAGS	+= -pg
# LLDFLAGS	+= -pg

default:	build-me

include $(BUILDRULES)

ifeq "$(TARGET_OS)" "linux"
build-me: domain.h $(LIBTARGET) $(CMDTARGET) $(HELPTARGETS)
	@if [ `grep -c $(CONF_LINE) ../pmcd.conf` -eq 0 ]; then \
		echo $(CONF_LINE) >> ../pmcd.conf ; \
	fi

install: default
	$(INSTALL) -m 755 -d $(PMDADIR)
	$(INSTALL) -m 644 domain.h help help.dir help.pag root root_proc $(PMDADIR)
	$(INSTALL) -m 755 $(LIBTARGET) $(CMDTARGET) $(SCRIPTS) $(PMDADIR)
	$(INSTALL) -m 644 root_proc $(PCP_VAR_DIR)/pmns/root_proc
	$(INSTALL) -m 644 linux_proc_migrate.conf $(PCP_VAR_DIR)/config/pmlogrewrite/linux_proc_migrate.conf
else
build-me:
install:
endif

default_pcp : default

install_pcp : install

$(HELPTARGETS) : help
	$(RUN_IN_BUILD_ENV) $(TOPDIR)/src/newhelp/newhelp -n root_proc -v 2 -o help < help

$(VERSION_SCRIPT):
	$(VERSION_SCRIPT_MAKERULE)

domain.h: ../../pmns/stdpmid
	$(DOMAIN_MAKERULE)

cgroups.o pmda.o: clusters.h
cgroups.o pmda.o:	cgroups.h
cgroups.o pmda.o proc_pid.o proc_runq.o:	proc_pid.h
pmda.o proc_runq.o:	proc_runq.h
filesys.o:	filesys.h
ksym.o pmda.o:		ksym.h
pmda.o:	domain.h
pmda.o:	indom.h
pmda.o:	getinfo.h
pmda.o:	$(VERSION_SCRIPT)
