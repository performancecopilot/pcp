#
# Copyright (c) 2018-2020 Red Hat.
# Copyright (c) 2011 SGI.  All Rights Reserved.
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

IAM		= nfsclient
PYSCRIPT	= pmda$(IAM).python
LDIRT		= domain.h root pmns $(IAM).log
DOMAIN		= NFSCLIENT

MAN_SECTION	= 1
MAN_PAGES	= pmda$(IAM).$(MAN_SECTION)
MAN_DEST	= $(PCP_MAN_DIR)/man$(MAN_SECTION)

PMDATMPDIR	= $(PCP_PMDAS_DIR)/$(IAM)
PMDAADMDIR	= $(PCP_PMDASADM_DIR)/$(IAM)
REWRITEDIR	= $(PCP_SYSCONF_DIR)/pmlogrewrite
REWRITEVARDIR	= $(PCP_VAR_DIR)/config/pmlogrewrite

default_pcp default: build-me

include $(BUILDRULES)

ifeq "$(HAVE_PYTHON)" "true"
build-me: check_domain
install_pcp install: default
	$(INSTALL) -m 755 -d $(PMDAADMDIR)
	$(INSTALL) -m 755 -d $(PMDATMPDIR)
	$(INSTALL) -m 755 -t $(PMDATMPDIR) Install Remove Upgrade $(PYSCRIPT) $(PMDAADMDIR)
	$(INSTALL) -m 644 -t $(REWRITEVARDIR)/nfsclient_migrate.conf migrate.conf $(REWRITEDIR)/nfsclient_migrate.conf
	@$(INSTALL_MAN)
else
build-me:
install_pcp install:
	@$(INSTALL_MAN)
endif

check_domain: ../../pmns/stdpmid
	$(DOMAIN_PYTHONRULE)

check:: $(PYSCRIPT)
	$(PYLINT) $^

check:: $(MAN_PAGES)
	$(MANLINT) $^
