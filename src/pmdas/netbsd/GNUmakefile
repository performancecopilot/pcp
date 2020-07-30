#
# Copyright (c) 2000,2003,2004,2008 Silicon Graphics, Inc.  All Rights Reserved.
# Copyright (c) 2007-2010 Aconex.  All Rights Reserved.
# Copyright (c) 2012 Ken McDonell.  All Rights Reserved.
# Copyright (c) 2015,2020 Red Hat.
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

IAM		= netbsd
DOMAIN		= NETBSD
CMDTARGET 	= pmdanetbsd
LIBTARGET	= pmda_netbsd.so

PMDATMPDIR	= $(PCP_PMDAS_DIR)/$(IAM)
PMDAADMDIR	= $(PCP_PMDASADM_DIR)/$(IAM)
REWRITEDIR	= $(PCP_SYSCONF_DIR)/pmlogrewrite
REWRITEVARDIR	= $(PCP_VAR_DIR)/config/pmlogrewrite
CONF_LINE	= "netbsd	116	dso	netbsd_init	$(PMDATMPDIR)/$(LIBTARGET)"

CFILES		= netbsd.c disk.c percpu.c netif.c swap.c vm_uvmexp.c filesys.c
HFILES		= netbsd.h
CONFTARGETS	= netbsd_disk_units.conf netbsd_misc.conf
LDIRT		= help.dir help.pag help.sed help.tmp domain.h $(IAM).log

LLDLIBS = $(PCP_PMDALIB)

default:	build-me

include $(BUILDRULES)

ifeq "$(TARGET_OS)" "netbsd"
build-me: $(LIBTARGET) $(CMDTARGET) help.dir help.pag
	@if [ -f ../pmcd.conf ]; then \
	    if [ `grep -c $(CONF_LINE) ../pmcd.conf` -eq 0 ]; then \
		echo $(CONF_LINE) >> ../pmcd.conf ; \
	    fi; \
	fi

install: default
	$(INSTALL) -m 755 -d $(PMDAADMDIR)
	$(INSTALL) -m 755 -d $(PMDATMPDIR)
	$(INSTALL) -m 644 -t $(PMDATMPDIR) domain.h help help.dir help.pag $(PMDAADMDIR)
	$(INSTALL) -m 755 -t $(PMDATMPDIR) $(LIBTARGET) $(CMDTARGET) $(PMDAADMDIR)
	$(INSTALL) -m 644 -t $(PCP_PMNS_DIR)/root_netbsd root_netbsd $(PCP_PMNSADM_DIR)/root_netbsd
	$(INSTALL) -m 644 -t $(LOGREWRITEVARDIR)/netbsd_disk_units.conf netbsd_disk_units.conf $(LOGREWRITEDIR)/netbsd_disk_units.conf
else
build-me:
install:
endif

help.dir help.pag : domain.h help
	$(SED) <domain.h >help.sed -n -e '/#define/s/#define[ 	]*\([A-Z][A-Z]*\)[ 	]*\([0-9][0-9]*\)/s@\1@\2@/p'
	$(SED) -f help.sed <help >help.tmp
	$(NEWHELP) -n root_netbsd -v 2 -o help <help.tmp

default_pcp : default

install_pcp : install

domain.h: ../../pmns/stdpmid
	$(DOMAIN_MAKERULE)

$(OBJECTS):	domain.h netbsd.h
