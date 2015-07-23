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

IAM		= netbsd
DOMAIN		= NETBSD
CMDTARGET 	= pmdanetbsd
LIBTARGET	= pmda_netbsd.so
PMDADIR	= $(PCP_PMDAS_DIR)/$(IAM)
CONF_LINE	= "netbsd	116	dso	netbsd_init	$(PMDADIR)/$(LIBTARGET)"

CFILES		= netbsd.c disk.c netif.c

HFILES		= netbsd.h

LSRCFILES	= help root_netbsd
LDIRT		= help.dir help.pag help.sed help.tmp domain.h $(IAM).log

LLDLIBS = $(PCP_PMDALIB) -lkvm

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
	$(INSTALL) -m 755 -d $(PMDADIR)
	$(INSTALL) -m 644 domain.h help help.dir help.pag $(PMDADIR)
	$(INSTALL) -m 755 $(LIBTARGET) $(CMDTARGET) $(PMDADIR)
	$(INSTALL) -m 644 root_netbsd $(PCP_VAR_DIR)/pmns/root_netbsd
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
