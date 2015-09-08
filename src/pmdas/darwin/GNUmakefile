#
# Copyright (c) 2004 Silicon Graphics, Inc.  All Rights Reserved.
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

IAM		= darwin
DOMAIN		= DARWIN
CMDTARGET	= pmdadarwin
LIBTARGET	= pmda_darwin.dylib
PMDADIR		= $(PCP_PMDAS_DIR)/$(IAM)
CONF_LINE	= "darwin	78	dso	darwin_init	$(PMDADIR)/$(LIBTARGET)"

CFILES		= pmda.c kernel.c disk.c network.c
HFILES		= disk.h network.h

LSRCFILES	= help root pmns

FRAMEWORKS	= -framework CoreFoundation -framework IOKit
LLDLIBS		= $(PCP_PMDALIB) $(FRAMEWORKS)

LDIRT		= *.log *.dir *.pag root_darwin domain.h

default:	build-me

include $(BUILDRULES)

ifeq "$(TARGET_OS)" "darwin" 
build-me: root_darwin $(LIBTARGET) $(CMDTARGET) help.dir help.pag
	@if [ -f ../pmcd.conf ]; then \
	    if [ `grep -c $(CONF_LINE) ../pmcd.conf` -eq 0 ]; then \
		echo $(CONF_LINE) >> ../pmcd.conf ; \
	    fi; \
	fi

install: build-me
	$(INSTALL) -m 755 -d $(PMDADIR)
	$(INSTALL) -m 644 domain.h help.dir help.pag $(PMDADIR)
	$(INSTALL) -m 755 $(LIBTARGET) $(CMDTARGET) $(PMDADIR)
	$(INSTALL) -m 644 root_darwin $(PCP_VAR_DIR)/pmns/root_darwin
else
build-me:
install:
endif

default_pcp : default

install_pcp : install

help.dir help.pag: help root_darwin
	$(NEWHELP) -n root_darwin -v 2 -o help < help

root_darwin: ../../pmns/stdpmid root pmns
	rm -f root_darwin
	sed -e 's;<stdpmid>;"../../pmns/stdpmid";' <root \
	| ( $(PMCPP) ) \
	| sed -e '/^#/d' -e '/^$$/d' >root_darwin

$(OBJECTS): domain.h

domain.h: ../../pmns/stdpmid
	$(DOMAIN_MAKERULE)
