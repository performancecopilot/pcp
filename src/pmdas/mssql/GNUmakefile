#
# Copyright (c) 2019-2021 Red Hat.
# Copyright (c) 2011 Aconex.  All Rights Reserved.
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

IAM		= mssql
PYSCRIPT	= pmda$(IAM).python
CONF		= $(IAM).conf
LDIRT		= domain.h root pmns $(IAM).log
DOMAIN		= MSSQL

ifneq ($(POD2MAN),)
MAN_SECTION = 1
MAN_PAGES = pmda$(IAM).$(MAN_SECTION)
MAN_DEST = $(PCP_MAN_DIR)/man$(MAN_SECTION)
endif

PMDAADMDIR	= $(PCP_PMDASADM_DIR)/$(IAM)
PMDACONFIG	= $(PCP_SYSCONF_DIR)/$(IAM)
PMDATMPDIR	= $(PCP_PMDAS_DIR)/$(IAM)
PMIEDIR		= $(PCP_SYSCONF_DIR)/pmieconf/$(IAM)
PMIEVARDIR	= $(PCP_VAR_DIR)/config/pmieconf/$(IAM)
LOGCONFDIR	= $(PCP_SYSCONF_DIR)/pmlogconf/$(IAM)
LOGCONFVARDIR	= $(PCP_VAR_DIR)/config/pmlogconf/$(IAM)

default_pcp default: build-me

include $(BUILDRULES)

ifeq "$(PMDA_MSSQL)" "true"
build-me: check_domain
install_pcp install: default
	$(INSTALL) -m 755 -d $(PMDAADMDIR)
	$(INSTALL) -m 755 -d $(PMDATMPDIR)
	$(INSTALL) -m 755 -t $(PMDATMPDIR) Install Remove Upgrade $(PYSCRIPT) $(PMDAADMDIR)
	$(INSTALL) -m 755 -d $(PMDACONFIG)
	$(INSTALL) -m 600 -t $(PMDATMPDIR)/$(CONF) $(CONF) $(PMDACONFIG)/$(CONF)
	$(INSTALL) -m 755 -d $(PMIEDIR)
	$(INSTALL) -m 755 -d $(PMIEVARDIR)
	$(INSTALL) -m 644 -t $(PMIEVARDIR)/stolen_node_memory mssql.stolen_node_memory.pmie $(PMIEDIR)/stolen_node_memory
	$(INSTALL) -m 755 -d $(LOGCONFDIR)
	$(INSTALL) -m 755 -d $(LOGCONFVARDIR)
	$(INSTALL) -m 644 -t $(LOGCONFVARDIR)/summary pmlogconf.summary $(LOGCONFDIR)/summary
	@$(INSTALL_MAN)
else
build-me:
install_pcp install:
	@$(INSTALL_MAN)
endif

check_domain:	../../pmns/stdpmid
	$(DOMAIN_PYTHONRULE)

check:: $(PYSCRIPT)
	$(PYLINT) $^

check:: $(MAN_PAGES)
	$(MANLINT) $^
