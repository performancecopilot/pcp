#
# Copyright (c) 2011, Nathan Scott.  All Rights Reserved.
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
WINDIR = $(TOPDIR)/src/win32ctl

IAM		= etw
DOMAIN		= ETW
CFILES		= util.c event.c pmda.c
HFILES		= util.h event.h
LSRCFILES	= tdhlist.c tdhconsume.c pcp.xml pmns root help
LCFLAGS		= -I$(WINDIR)/include -D_WIN32_WINNT=0x0600 -DFORCEINLINE="static inline"
LLDFLAGS	= -L$(WINDIR)/lib
LLDLIBS		= $(PCP_PMDALIB) -lpcp_tdh

PMDADIR		= $(PCP_PMDAS_DIR)/$(IAM)
LIBTARGET	= pmda_etw.dll
EXTRATARGETS	= tdhlist.exe tdhconsume.exe help.dir help.pag
LDIRT		= $(EXTRATARGETS) root_etw domain.h $(IAM).log

CONF_LINE = "etw	87	dso	etw_init	$(PCP_PMDAS_DIR)/etw/pmda_etw.dll"

default:	build-me

include $(BUILDRULES)

ifeq "$(TARGET_OS)" ""# "mingw"
build-me: root_etw $(LSRCFILES) $(LIBTARGET) $(EXTRATARGETS)
	@if [ -f ../pmcd.conf ]; then \
	    if [ `grep -c $(CONF_LINE) ../pmcd.conf` -eq 0 ]; then \
		echo $(CONF_LINE) >> ../pmcd.conf ; \
	    fi; \
	fi

install: build-me
	$(INSTALL) -m 755 -d $(PMDADIR)
	$(INSTALL) -m 755 tdhlist.exe $(PMDADIR)/tdhlist.exe
	$(INSTALL) -m 644 pmns $(PMDADIR)/root
	$(INSTALL) -m 644 domain.h help.dir help.pag help $(PMDADIR)
	$(INSTALL) -m 644 root_etw $(PCP_VAR_DIR)/pmns/root_etw
else
build-me:
install:
endif

help.dir help.pag : help root_etw
	$(NEWHELP) -n root_etw -v 2 -o help < help

default_pcp:	default

install_pcp:	install

root_etw:	../../pmns/stdpmid $(PMNS)
	rm -f root_etw
	sed -e 's;<stdpmid>;"../../pmns/stdpmid";' <root \
	| ( $(PMCPP) ) \
	| sed -e '/^#/d' -e '/^$$/d' >root_etw

domain.h: ../../pmns/stdpmid
	$(DOMAIN_MAKERULE)

$(OBJECTS):	domain.h util.h

tdhlist.exe:	tdhlist.o util.o
	$(CCF) -o tdhlist.exe tdhlist.o util.o $(LLDFLAGS) -lpcp_tdh

tdhconsume.exe:	tdhconsume.o util.o
	$(CCF) -o tdhconsume.exe tdhconsume.o util.o $(LLDFLAGS) -lpcp_tdh -lwsock32
