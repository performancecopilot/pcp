#
# Copyright (c) 2000,2003,2004,2008 Silicon Graphics, Inc.  All Rights Reserved.
# Copyright (c) 2007-2010 Aconex.  All Rights Reserved.
# Copyright (c) 2013-2016,2019-2021 Red Hat.
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
PMDATMPDIR	= $(PCP_PMDAS_DIR)/$(IAM)
PMDACONFIG	= $(PCP_SYSCONF_DIR)/$(IAM)
PMDAADMDIR	= $(PCP_PMDASADM_DIR)/$(IAM)
REWRITEDIR	= $(PCP_SYSCONF_DIR)/pmlogrewrite
REWRITEVARDIR	= $(PCP_VAR_DIR)/config/pmlogrewrite
CONF_LINE	= "proc	3	pipe	binary		$(PMDATMPDIR)/$(CMDTARGET) -d 3"

CFILES		= pmda.c acct.c cgroups.c contexts.c proc_pid.c proc_dynamic.c \
		  getinfo.c gram_node.c config.c error.c hotproc.c

HFILES		= clusters.h indom.h config.h contexts.h hotproc.h gram_node.h \
		  acct.h cgroups.h proc_pid.h getinfo.h

LFILES		= lex.l
YFILES		= gram.y

SCRIPTS		= Install Remove
LOGREWRITERS	= linux_proc_migrate.conf cgroup_units.conf \
		  proc_kernel_ulong.conf proc_jiffies.conf proc_scheduler.conf \
		  proc_discrete_strings.conf proc_kernel_ulong_migrate.conf
VERSION_SCRIPT	= exports
HELPTARGETS	= help.dir help.pag
LDIRT		= $(HELPTARGETS) domain.h $(VERSION_SCRIPT) $(YFILES:%.y=%.tab.?) \
		  proc_kernel_ulong.conf proc_jiffies.conf proc_kernel_ulong_migrate.conf

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
build-me: root_proc $(LIBTARGET) $(CMDTARGET) $(HELPTARGETS) $(LOGREWRITERS)
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
	$(INSTALL) -m 644 -t $(PMDATMPDIR)/samplehotproc.conf samplehotproc.conf $(PMDACONFIG)/samplehotproc.conf
	$(INSTALL) -m 755 -t $(PMDATMPDIR) $(LIBTARGET) $(CMDTARGET) $(SCRIPTS) $(PMDAADMDIR)
	$(INSTALL) -m 644 -t $(PCP_PMNS_DIR)/root_proc root_proc $(PCP_PMNSADM_DIR)/root_proc
	$(INSTALL) -m 644 -t $(REWRITEVARDIR) $(LOGREWRITERS) $(REWRITEDIR)
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
	$(NEWHELP) -n root_proc -v 2 -o help < help

$(VERSION_SCRIPT):
	$(VERSION_SCRIPT_MAKERULE)

domain.h: ../../pmns/stdpmid
	$(DOMAIN_MAKERULE)

$(OBJECTS): domain.h

proc_kernel_ulong.conf proc_jiffies.conf proc_kernel_ulong_migrate.conf: mk.rewrite
	CPP="$(CPP)" INCDIR="$(TOPDIR)/src/include" ./mk.rewrite

gram.tab.o:	hotproc.h
lex.o:	hotproc.h
acct.o pmda.o: acct.h
cgroups.o pmda.o: clusters.h
cgroups.o pmda.o:	cgroups.h
cgroups.o pmda.o proc_pid.o proc_dynamic.o:	proc_pid.h
proc_dynamic.o:	help_text.h
indom.o pmda.o:	indom.h
pmda.o:	domain.h
pmda.o:	getinfo.h
pmda.o:	$(VERSION_SCRIPT)

acct.o cgroups.o contexts.o pmda.o proc_dynamic.o proc_pid.o:	$(TOPDIR)/src/include/pcp/libpcp.h

check:: $(MAN_PAGES)
	$(MANLINT) $^
