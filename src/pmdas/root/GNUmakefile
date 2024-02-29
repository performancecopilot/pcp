#
# Copyright (c) 2014-2015,2018,2020 Red Hat.
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

IAM		= root
DOMAIN		= ROOT
CMDTARGET	= pmda$(IAM)$(EXECSUFFIX)

PMDATMPDIR	= $(PCP_PMDAS_DIR)/$(IAM)
PMDAADMDIR	= $(PCP_PMDASADM_DIR)/$(IAM)
CONF_LINE	= "root	1	pipe	binary		$(PMDATMPDIR)/$(CMDTARGET)"

JSONSL_HFILES   = $(addprefix deps/jsonsl/, jsonsl.h)
JSONSL_CFILES   = $(addprefix deps/jsonsl/, jsonsl.c)
JSONSL_XFILES   = $(JSONSL_HFILES) $(JSONSL_CFILES)

HFILES		= $(JSONSL_HFILES)
CFILES		= $(JSONSL_CFILES) root.c agent.c
ifeq "$(TARGET_OS)" "linux"
CFILES		+= podman.c docker.c lxc.c
endif

LCFLAGS		= -Ideps

VERSION_SCRIPT	= exports
HELPTARGETS	= help.dir help.pag
LDIRT		= $(HELPTARGETS) pmns domain.h $(VERSION_SCRIPT) $(JSONSL_XFILES)

LLDLIBS		= $(PCP_WEBLIB)

default:	$(JSONSL_XFILES) $(CMDTARGET) $(HELPTARGETS) pmns
ifneq "$(TARGET_OS)" "mingw"
	@if [ -f ../pmcd.conf ]; then \
	    if [ `grep -c $(CONF_LINE) ../pmcd.conf` -eq 0 ]; then \
		echo $(CONF_LINE) >> ../pmcd.conf ; \
	    fi; \
	fi
endif

include $(BUILDRULES)

install: default
	$(INSTALL) -m 755 -d $(PMDAADMDIR)
	$(INSTALL) -m 755 -d $(PMDATMPDIR)
	$(INSTALL) -m 644 -t $(PMDATMPDIR) domain.h help help.dir help.pag root $(PMDAADMDIR)
	$(INSTALL) -m 755 -t $(PMDATMPDIR)/$(CMDTARGET) $(CMDTARGET) $(PMDAADMDIR)/$(CMDTARGET)
	$(INSTALL) -m 644 -t $(PMDATMPDIR)/root_root pmns $(PMDAADMDIR)/root_root
	$(INSTALL) -m 644 -t $(PCP_PMNS_DIR)/root_root root_root $(PCP_PMNSADM_DIR)/root_root

default_pcp : default

install_pcp : install

$(HELPTARGETS) : help
	$(NEWHELP) -n root_root -v 2 -o help < help

$(VERSION_SCRIPT):
	$(VERSION_SCRIPT_MAKERULE)

$(JSONSL_XFILES):
	mkdir -p deps/jsonsl
	$(LN_S) -f $(realpath $(TOPDIR))/vendor/github.com/mnunberg/$(@:deps/%=%) $@

$(OBJECTS): domain.h

domain.h: ../../pmns/stdpmid
	$(DOMAIN_MAKERULE)

pmns :
	$(LN_S) -f root_root pmns

lxc.o root.o:	$(TOPDIR)/src/include/pcp/libpcp.h
podman.o:	$(JSONSL_HFILES)
