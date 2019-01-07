#
# Copyright (c) 2000,2003,2004,2008 Silicon Graphics, Inc.  All Rights Reserved.
# Copyright (c) 2007-2010 Aconex.  All Rights Reserved.
# Copyright (c) 2012 Ken McDonell.  All Rights Reserved.
# Copyright (c) 2015 Red Hat.
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

IAM		= openbsd
DOMAIN		= OPENBSD
CMDTARGET 	= pmdaopenbsd
LIBTARGET	= pmda_openbsd.so
PMDADIR	= $(PCP_PMDAS_DIR)/$(IAM)
LOGREWRITEDIR   = $(PCP_VAR_DIR)/config/pmlogrewrite
CONF_LINE	= "openbsd	139	dso	openbsd_init	$(PMDADIR)/$(LIBTARGET)"

CFILES		= openbsd.c disk.c percpu.c netif.c swap.c vm_uvmexp.c \
		  filesys.c

HFILES		= openbsd.h
CONFTARGETS	= openbsd_disk_units.conf openbsd_misc.conf

LSRCFILES	= help root_openbsd
LDIRT		= help.dir help.pag help.sed help.tmp domain.h $(IAM).log

LLDLIBS = $(PCP_PMDALIB) -lkvm

default:	build-me

include $(BUILDRULES)

ifeq "$(TARGET_OS)" "openbsd"
build-me: $(LIBTARGET) $(CMDTARGET) help.dir help.pag $(CONFTARGETS)
	@if [ -f ../pmcd.conf ]; then \
	    if [ `grep -c $(CONF_LINE) ../pmcd.conf` -eq 0 ]; then \
		echo $(CONF_LINE) >> ../pmcd.conf ; \
	    fi; \
	fi

install: default
	$(INSTALL) -m 755 -d $(PMDADIR)
	$(INSTALL) -m 644 domain.h help help.dir help.pag $(PMDADIR)
	$(INSTALL) -m 755 $(LIBTARGET) $(CMDTARGET) $(PMDADIR)
	$(INSTALL) -m 644 root_openbsd $(PCP_VAR_DIR)/pmns/root_openbsd
	$(INSTALL) -m 644 openbsd_disk_units.conf $(LOGREWRITEDIR)/openbsd_disk_units.conf
else
build-me:
install:
endif

help.dir help.pag : domain.h help
	$(SED) <domain.h >help.sed -n -e '/#define/s/#define[ 	]*\([A-Z][A-Z]*\)[ 	]*\([0-9][0-9]*\)/s@\1@\2@/p'
	$(SED) -f help.sed <help >help.tmp
	$(NEWHELP) -n root_openbsd -v 2 -o help <help.tmp

default_pcp : default

install_pcp : install

domain.h: ../../pmns/stdpmid
	$(DOMAIN_MAKERULE)

$(OBJECTS):	domain.h openbsd.h
