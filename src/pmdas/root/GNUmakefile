#
# Copyright (c) 2014-2015 Red Hat.
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
PMDADIR		= $(PCP_PMDAS_DIR)/$(IAM)
CONF_LINE	= "root	1	pipe	binary		$(PMDADIR)/$(CMDTARGET)"

CFILES		= root.c
ifeq "$(TARGET_OS)" "linux"
CFILES		+= jsmn.c docker.c lxc.c
endif

SCRIPTS		= Install Remove
VERSION_SCRIPT	= exports
HELPTARGETS	= help.dir help.pag
LDIRT		= $(HELPTARGETS) pmns domain.h $(VERSION_SCRIPT)

LCFLAGS		= -DJSMN_PARENT_LINKS
LLDLIBS		= $(PCP_PMDALIB)

default:	$(CMDTARGET) $(HELPTARGETS) pmns
	@if [ -f ../pmcd.conf ]; then \
	    if [ `grep -c $(CONF_LINE) ../pmcd.conf` -eq 0 ]; then \
		echo $(CONF_LINE) >> ../pmcd.conf ; \
	    fi; \
	fi

include $(BUILDRULES)

install: default
	$(INSTALL) -m 755 -d $(PMDADIR)
	$(INSTALL) -m 644 domain.h help help.dir help.pag root $(PMDADIR)
	$(INSTALL) -m 755 $(CMDTARGET) $(SCRIPTS) $(PMDADIR)
	$(INSTALL) -m 644 pmns $(PMDADIR)/root_root
	$(INSTALL) -m 644 root_root $(PCP_VAR_DIR)/pmns/root_root

default_pcp : default

install_pcp : install

$(HELPTARGETS) : help
	$(NEWHELP) -n root_root -v 2 -o help < help

$(VERSION_SCRIPT):
	$(VERSION_SCRIPT_MAKERULE)

$(OBJECTS): domain.h

domain.h: ../../pmns/stdpmid
	$(DOMAIN_MAKERULE)

pmns :
	$(LN_S) -f root_root pmns
