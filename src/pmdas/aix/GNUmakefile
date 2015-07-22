#
# Copyright (c) 2000,2003,2004 Silicon Graphics, Inc.  All Rights Reserved.
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

IAM	= aix
DOMAIN	= AIX
CMDTARGET = pmdaaix
LIBTARGET = pmda_aix.so
PMDADIR   = $(PCP_PMDAS_DIR)/$(IAM)
CONF_LINE = "aix	80	dso	aix_init	$(PMDADIR)/$(LIBTARGET)"

CFILES	= aix.c data.c disk_total.c disk.c cpu_total.c cpu.c netif.c

LLDLIBS = $(PCP_PMDALIB) -lperfstat
PMNS	= pmns.disk pmns.kernel pmns.mem pmns.network pmns.hinv
LSRCFILES = $(PMNS) help root common.h

LDIRT	= *.log *.dir *.pag root_aix domain.h

default:	build-me

ifeq "$(TARGET_OS)" "aix" 
build-me: common.h root_aix $(CMDTARGET) $(LIBTARGET) help.dir help.pag
	@if [ -f ../pmcd.conf ]; then \
	    if [ `grep -c $(CONF_LINE) ../pmcd.conf` -eq 0 ]; then \
		echo $(CONF_LINE) >> ../pmcd.conf ; \
	    fi; \
	fi

install: build-me
	$(INSTALL) -m 755 -d $(PMDADIR)
	$(INSTALL) -m 644 domain.h help.dir help.pag $(PMDADIR)
	$(INSTALL) -m 755 $(LIBTARGET) $(CMDTARGET) $(PMDADIR)
	$(INSTALL) -m 644 root_aix $(PCP_VAR_DIR)/pmns/root_aix
else
build-me:
install:
endif

$(OBJECTS):	common.h

include $(BUILDRULES)

default_pcp : default

install_pcp : install

help.dir help.pag: help root_aix
	$(NEWHELP) -n root_aix -v 2 -o help < help

root_aix:	../../pmns/stdpmid
	rm -f root_aix
	sed -e 's;<stdpmid>;"../../pmns/stdpmid";' <root \
	| ( $(PMCPP) ) \
	| sed -e '/^#/d' -e '/^$$/d' >root_aix

$(OBJECTS): domain.h

domain.h:	../../pmns/stdpmid
	$(DOMAIN_MAKERULE)
