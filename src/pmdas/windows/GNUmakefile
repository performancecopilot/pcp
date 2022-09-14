#
# Copyright (c) 2020,2022 Red Hat.  All Rights Reserved.
# Copyright (c) 2008-2009 Aconex.  All Rights Reserved.
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
WINDIR = $(TOPDIR)/src/win32ctl

IAM		= windows
DOMAIN		= WINDOWS
CFILES		= pmda.c error.c open.c instance.c fetch.c helptext.c
HFILES		= hypnotoad.h
LCFLAGS		= -DWIN32_LEAN_AND_MEAN -D_WIN32_WINNT=0x0500 \
		  -I$(WINDIR)/include
LLDFLAGS	= -L$(WINDIR)/lib
LLDLIBS		= $(PCP_PMDALIB) -lpdh
PMNS		= pmns.disk pmns.kernel pmns.mem pmns.network \
		  pmns.sqlserver pmns.filesys pmns.hinv pmns.pmda \
		  pmns.process
LIBTARGET	= pmda_windows.dll
EXTRATARGETS	= pdhlist.exe help.dir help.pag

LDIRT		= root_windows domain.h $(IAM).log pmda$(IAM) \
		  $(EXTRATARGETS) pdhlist.o

PMDATMPDIR 	= $(PCP_PMDAS_DIR)/$(IAM)
PMDAADMDIR 	= $(PCP_PMDASADM_DIR)/$(IAM)
LOGCONFDIR	= $(PCP_SYSCONF_DIR)/pmlogconf
LOGCONFVARDIR	= $(PCP_VAR_DIR)/config/pmlogconf

CONF_LINE	= "$(IAM)	79	dso	$(IAM)_init	$(PMDATMPDIR)/$(LIBTARGET)"

default:	build-me

include $(BUILDRULES)

ifeq "$(TARGET_OS)" "mingw"
build-me: root_windows $(LIBTARGET) $(EXTRATARGETS)
	@if [ -f ../pmcd.conf ]; then \
	    if [ `grep -c $(CONF_LINE) ../pmcd.conf` -eq 0 ]; then \
		echo $(CONF_LINE) >> ../pmcd.conf ; \
	    fi; \
	fi

install: build-me configs
	$(INSTALL) -m 755 -d $(PMDAADMDIR)
	$(INSTALL) -m 755 -d $(PMDATMPDIR)
	$(INSTALL) -m 755 -t $(PMDATMPDIR) pdhlist.exe $(LIBTARGET) $(PMDAADMDIR)
	$(INSTALL) -m 644 -t $(PMDATMPDIR) README root $(PMNS) $(PMDAADMDIR)
	$(INSTALL) -m 644 -t $(PMDATMPDIR) domain.h help.dir help.pag help $(PMDAADMDIR)
	$(INSTALL) -m 644 -t $(PCP_PMNS_DIR)/root_windows root_windows $(PCP_PMNSADM_DIR)/root_windows
else
build-me:
install: configs
endif

configs:
	$(INSTALL) -m 755 -d $(LOGCONFDIR)/sqlserver
	$(INSTALL) -m 755 -d $(LOGCONFVARDIR)/sqlserver
	$(INSTALL) -m 644 -t $(LOGCONFVARDIR)/sqlserver/summary pmlogconf.sqlserver $(LOGCONFDIR)/sqlserver/summary

help.dir help.pag : help root_windows
	$(NEWHELP) -n root_windows -v 2 -o help < help

default_pcp:	default

install_pcp:	install

root_windows:	../../pmns/stdpmid $(PMNS)
	rm -f root_windows
	sed -e 's;<stdpmid>;"../../pmns/stdpmid";' <root \
	| ( $(PMCPP) ) \
	| sed -e '/^#/d' -e '/^$$/d' >root_windows

domain.h: ../../pmns/stdpmid
	$(DOMAIN_MAKERULE)

$(OBJECTS):	hypnotoad.h domain.h

pdhlist.exe:	pdhlist.o error.o
	$(CCF) -o pdhlist.exe pdhlist.o error.o $(LLDFLAGS) -L$(TOPDIR)/src/libpcp/src -lpcp -lpdh

$(OBJECTS):	$(TOPDIR)/src/include/pcp/libpcp.h
