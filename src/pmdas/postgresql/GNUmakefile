#
# Copyright (c) 2018-2020,2024 Red Hat.
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

IAM		= postgresql
PYSCRIPT	= pmda$(IAM).python
CONF		= pmda$(IAM).conf
LDIRT		= domain.h root pmns $(IAM).log
DOMAIN		= POSTGRESQL

PMDAADMDIR	= $(PCP_PMDASADM_DIR)/$(IAM)
PMDACONFIG	= $(PCP_SYSCONF_DIR)/$(IAM)
PMDATMPDIR	= $(PCP_PMDAS_DIR)/$(IAM)
LOGCONFDIR	= $(PCP_SYSCONF_DIR)/pmlogconf/$(IAM)
LOGCONFVARDIR	= $(PCP_VAR_DIR)/config/pmlogconf/$(IAM)

MAN_SECTION	= 1
MAN_PAGES	= pmda$(IAM).$(MAN_SECTION)
MAN_DEST	= $(PCP_MAN_DIR)/man$(MAN_SECTION)

default_pcp default: build-me

include $(BUILDRULES)

ifeq "$(PMDA_POSTGRESQL)" "true"
build-me: check_domain
install_pcp install: default
	$(INSTALL) -m 755 -d $(PMDAADMDIR)
	$(INSTALL) -m 755 -d $(PMDATMPDIR)
	$(INSTALL) -m 755 -t $(PMDATMPDIR) Install Remove Upgrade $(PYSCRIPT) $(PMDAADMDIR)
	$(INSTALL) -m 755 -d $(PMDACONFIG)
	$(INSTALL) -m 600 -t $(PMDATMPDIR)/$(CONF) $(CONF) $(PMDACONFIG)/$(CONF)
	@$(INSTALL_MAN)
	$(INSTALL) -m 755 -d $(LOGCONFDIR)
	$(INSTALL) -m 755 -d $(LOGCONFVARDIR)
	$(INSTALL) -m 644 -t $(LOGCONFVARDIR)/summary pmlogconf.summary $(LOGCONFDIR)/summary
	$(INSTALL) -m 755 $(IAM).pmcheck $(PCP_SHARE_DIR)/lib/pmcheck/pmda-$(IAM)
else
build-me:
install_pcp install: $(TOPDIR)/src/pmcheck/pmda.na.template
	$(INSTALL) -m 755 $< $(PCP_SHARE_DIR)/lib/pmcheck/pmda-$(IAM)
	@$(INSTALL_MAN)
endif

check_domain: ../../pmns/stdpmid
	$(DOMAIN_PYTHONRULE)

check:: $(PYSCRIPT)
	$(PYLINT) $^

check:: $(MAN_PAGES)
	$(MANLINT) $^
