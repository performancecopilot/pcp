#
# Copyright (c) 2013-2015,2020 Red Hat.
# Copyright (c) 2000,2003,2004 Silicon Graphics, Inc.  All Rights Reserved.
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
include $(TOPDIR)/src/include/builddefs

IAM	= solaris
DOMAIN	= SOLARIS
PMDAINIT = solaris_init
CMDTARGET = pmdasolaris
LIBTARGET = pmda_solaris.so

PMDATMPDIR = $(PCP_PMDAS_DIR)/$(IAM)
PMDAADMDIR = $(PCP_PMDASADM_DIR)/$(IAM)
CONF_LINE = "solaris	75	dso	$(PMDAINIT)	$(PMDATMPDIR)/$(LIBTARGET)"

CFILES	= solaris.c data.c sysinfo.c disk.c zpool.c zfs.c \
	 zpool_perdisk.c netmib2.c netlink.c kvm.c arcstats.c vnops.c

BARE_NS = disk kernel mem network hinv zpool zfs zpool_perdisk
PMNS	= $(BARE_NS:%=pmns.%)

HELPTARGETS = help.dir help.pag
VERSION_SCRIPT = exports

LDIRT	= domain.h *.log $(HELPTARGETS) root_solaris $(VERSION_SCRIPT)

LLDLIBS = $(PCP_PMDALIB) -lkstat -lzfs -lnvpair -lkvm -ldevinfo

default: build-me

include $(BUILDRULES)

ifeq "$(TARGET_OS)" "solaris"
build-me: root_solaris $(LIBTARGET) $(CMDTARGET) $(HELPTARGETS) $(VERSION_SCRIPT)
	@if [ -f ../pmcd.conf ]; then \
	    if [ `grep -c $(CONF_LINE) ../pmcd.conf` -eq 0 ]; then \
		echo $(CONF_LINE) >> ../pmcd.conf ; \
	    fi; \
	fi

install: build-me
	$(INSTALL) -m 755 -d $(PMDAADMDIR)
	$(INSTALL) -m 755 -d $(PMDATMPDIR)
	$(INSTALL) -m 644 -t $(PMDATMPDIR) domain.h $(HELPTARGETS) $(PMDAADMDIR)
	$(INSTALL) -m 755 -t $(PMDATMPDIR) $(LIBTARGET) $(CMDTARGET) $(PMDAADMDIR)
	$(INSTALL) -m 644 -t $(PCP_PMNS_DIR)/root_solaris root_solaris $(PCP_PMNSADM_DIR)/root_solaris
else
build-me:
install:
endif

default_pcp : default

install_pcp : install

$(OBJECTS):	common.h domain.h

$(HELPTARGETS): help root_solaris
	$(NEWHELP) -n root_solaris -v 2 -o help < help

root_solaris: ../../pmns/stdpmid $(PMNS) root
	rm -f root_solaris
	sed -e 's;<stdpmid>;"../../pmns/stdpmid";' <root \
	| ( $(PMCPP) ) \
	| sed -e '/^#/d' -e '/^$$/d' >root_solaris

domain.h: ../../pmns/stdpmid
	$(DOMAIN_MAKERULE)

$(VERSION_SCRIPT):
	$(VERSION_SCRIPT_MAKERULE)

$(LIBTARGET): $(VERSION_SCRIPT)
