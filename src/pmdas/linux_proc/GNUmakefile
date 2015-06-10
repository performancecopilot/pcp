#
# Copyright (c) 2000,2003,2004,2008 Silicon Graphics, Inc.  All Rights Reserved.
# Copyright (c) 2007-2010 Aconex.  All Rights Reserved.
# Copyright (c) 2013-2015 Red Hat.
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

CFILES		= pmda.c cgroups.c proc_pid.c proc_runq.c proc_dynamic.c\
		  ksym.c getinfo.c contexts.c gram_node.c config.c error.c hotproc.c

HFILES		= clusters.h indom.h \
		  cgroups.h proc_pid.h proc_runq.h ksym.h getinfo.h contexts.h hotproc.h gram_node.h config.h

LFILES		= lex.l
YFILES		= gram.y

SCRIPTS		= Install Remove
LOGREWRITERS	= linux_proc_migrate.conf cgroup_units.conf proc_kernel_ulong.conf proc_jiffies.conf
VERSION_SCRIPT	= exports
HELPTARGETS	= help.dir help.pag
LDIRT		= $(HELPTARGETS) domain.h $(VERSION_SCRIPT) $(YFILES:%.y=%.tab.?) \
		  proc_kernel_ulong.conf proc_jiffies.conf

LLDLIBS		= $(PCP_PMDALIB)
LCFLAGS		= $(INVISIBILITY)

# Uncomment these flags for profiling
# LCFLAGS	+= -pg
# LLDFLAGS	+= -pg

MAN_SECTION	= 1
MAN_PAGES	= pmda$(IAM).$(MAN_SECTION)
MAN_DEST	= $(PCP_MAN_DIR)/man$(MAN_SECTION)

default:	build-me

include $(BUILDRULES)

ifeq "$(TARGET_OS)" "linux"
build-me: domain.h root_proc $(LIBTARGET) $(CMDTARGET) $(HELPTARGETS) $(LOGREWRITERS)
	@if [ -f ../pmcd.conf ]; then \
	    if [ `grep -c $(CONF_LINE) ../pmcd.conf` -eq 0 ]; then \
		echo $(CONF_LINE) >> ../pmcd.conf ; \
	    fi; \
	fi

install: default
	$(INSTALL) -m 755 -d $(PMDADIR)
	$(INSTALL) -m 644 domain.h help help.dir help.pag root root_proc samplehotproc.conf $(PMDADIR)
	$(INSTALL) -m 755 $(LIBTARGET) $(CMDTARGET) $(SCRIPTS) $(PMDADIR)
	$(INSTALL) -m 644 root_proc $(PCP_VAR_DIR)/pmns/root_proc
	$(INSTALL) -m 644 $(LOGREWRITERS) $(PCP_VAR_DIR)/config/pmlogrewrite
	@$(INSTALL_MAN)
else
build-me:
install:
endif

.NOTPARALLEL:
gram.tab.h gram.tab.c:	gram.y

config.o lex.o gram.tab.o:		gram.tab.h

default_pcp : default

install_pcp : install

$(HELPTARGETS) : help
	$(RUN_IN_BUILD_ENV) $(TOPDIR)/src/newhelp/newhelp -n root_proc -v 2 -o help < help

$(VERSION_SCRIPT):
	$(VERSION_SCRIPT_MAKERULE)

domain.h: ../../pmns/stdpmid
	$(DOMAIN_MAKERULE)

proc_kernel_ulong.conf proc_jiffies.conf:	mk.rewrite
	CPP="$(CPP)" INCDIR="$(TOPDIR)/src/include" ./mk.rewrite

gram.tab.o:	hotproc.h
lex.o:	hotproc.h
cgroups.o pmda.o: clusters.h
cgroups.o pmda.o:	cgroups.h
cgroups.o pmda.o proc_pid.o proc_runq.o:	proc_pid.h
pmda.o proc_runq.o:	proc_runq.h
indom.o pmda.o:	indom.h
ksym.o pmda.o:		ksym.h
pmda.o:	domain.h
pmda.o:	getinfo.h
pmda.o:	$(VERSION_SCRIPT)
