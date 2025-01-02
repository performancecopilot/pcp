#
# Copyright (c) 2017,2020 Red Hat.
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

IAM = bind2
DOMAIN = BIND2
PMDATMPDIR = $(PCP_PMDAS_DIR)/$(IAM)
PMDAADMDIR = $(PCP_PMDASADM_DIR)/$(IAM)
CONFIGDIR = $(PCP_SYSCONF_DIR)/$(IAM)
REWRITEDIR	= $(PCP_SYSCONF_DIR)/pmlogrewrite
REWRITEVARDIR	= $(PCP_VAR_DIR)/config/pmlogrewrite

LDIRT = domain.h root pmns *.log rewrite.conf

MAN_SECTION = 1
MAN_PAGES = pmda$(IAM).$(MAN_SECTION)
MAN_DEST = $(PCP_MAN_DIR)/man$(MAN_SECTION)

default_pcp default: check_domain rewrite.conf

include $(BUILDRULES)

ifeq "$(PMDA_BIND2)" "true"
install: default
	$(INSTALL) -m 755 -d $(PMDAADMDIR)
	$(INSTALL) -m 755 -d $(PMDATMPDIR)
	$(INSTALL) -m 755 -t $(PMDATMPDIR) Install Remove $(PMDAADMDIR)
	$(INSTALL) -m 644 -t $(PMDATMPDIR) pmda$(IAM).pl README.md $(PMDAADMDIR)
	$(INSTALL) -m 755 -d $(CONFIGDIR)
	$(INSTALL) -m 644 -t $(PMDATMPDIR)/bind2.conf bind2-example.conf $(CONFIGDIR)/bind2.conf
	@$(INSTALL_MAN)
	$(INSTALL) -m 644 -t $(REWRITEVARDIR)/bind2.conf rewrite.conf $(REWRITEDIR)/bind2.conf

install_pcp : install
else
install_pcp install:
	@$(INSTALL_MAN)
endif

rewrite.conf:	mk.rewrite pmda$(IAM).pl
	./mk.rewrite

check_domain:	../../pmns/stdpmid
	$(DOMAIN_PERLRULE)

check:: $(MAN_PAGES)
	$(MANLINT) $^
