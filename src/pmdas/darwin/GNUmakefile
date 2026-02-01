#
# Copyright (c) 2004 Silicon Graphics, Inc.  All Rights Reserved.
# Copyright (c) 2015,2020,2025 Red Hat.
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
PMDATMPDIR	= $(PCP_PMDAS_DIR)/$(IAM)
PMDAADMDIR	= $(PCP_PMDASADM_DIR)/$(IAM)
CONF_LINE	= "darwin	78	dso	darwin_init	$(PMDATMPDIR)/$(LIBTARGET)"

MAN_SECTION	= 1
MAN_PAGES	= pmda$(IAM).$(MAN_SECTION)
MAN_DEST	= $(PCP_MAN_DIR)/man$(MAN_SECTION)

CFILES		= pmda.c kernel.c disk.c network.c nfs.c vfs.c udp.c icmp.c ipv6.c sockstat.c tcpconn.c tcp.c vmstat.c filesys.c cpu.c loadavg.c cpuload.c uname.c version.c gpu.c gpu_iokit.c ipc.c power.c metrics.c
HFILES		= disk.h network.h nfs.h vfs.h udp.h icmp.h ipv6.h sockstat.h tcpconn.h tcp.h vmstat.h filesys.h cpu.h loadavg.h cpuload.h uname.h gpu.h ipc.h power.h metrics.h

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
	$(INSTALL) -m 755 -d $(PMDAADMDIR)
	$(INSTALL) -m 755 -d $(PMDATMPDIR)
	$(INSTALL) -m 755 -d $(PCP_PMNSADM_DIR)
	$(INSTALL) -m 755 -d $(PCP_PMNS_DIR)
	$(INSTALL) -m 644 -t $(PMDATMPDIR) domain.h help.dir help.pag $(PMDAADMDIR)
	$(INSTALL) -m 755 -t $(PMDATMPDIR) $(LIBTARGET) $(CMDTARGET) $(PMDAADMDIR)
	$(INSTALL) -m 644 -t $(PCP_PMNS_DIR)/root_darwin root_darwin $(PCP_PMNSADM_DIR)/root_darwin
	@$(INSTALL_MAN)
else
build-me:
install:
endif

default_pcp : default

install_pcp : install

check:: $(MAN_PAGES)
	$(MANLINT) $^

# Unit tests (no installation required)
test: build-me
	@echo "Running Darwin PMDA unit tests..."
	@cd test && ./run-unit-tests.sh

.PHONY: test

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
