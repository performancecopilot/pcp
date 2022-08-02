#
# Copyright (c) 2020,2022 Red Hat.
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

IAM	= openvswitch
PYSCRIPT = pmda$(IAM).python
LDIRT	= domain.h root pmns $(IAM).log
DOMAIN	= OPENVSWITCH

MAN_SECTION = 1
MAN_PAGES = pmda$(IAM).$(MAN_SECTION)
MAN_DEST = $(PCP_MAN_DIR)/man$(MAN_SECTION)

PMDAADMDIR = $(PCP_PMDASADM_DIR)/$(IAM)
PMDATMPDIR = $(PCP_PMDAS_DIR)/$(IAM)
LOGCONFDIR = $(PCP_SYSCONF_DIR)/pmlogconf/$(IAM)
LOGCONFVARDIR = $(PCP_VAR_DIR)/config/pmlogconf/$(IAM)
PMIECONFDIR = $(PCP_SYSCONF_DIR)/pmieconf/$(IAM)
PMIECONFVARDIR = $(PCP_VAR_DIR)/config/pmieconf/$(IAM)

default_pcp default:	build-me

include $(BUILDRULES)

ifeq "$(HAVE_PYTHON)" "true"
build-me:	check_domain
install_pcp install:	default 
	$(INSTALL) -m 755 -d $(PMDAADMDIR)
	$(INSTALL) -m 755 -d $(PMDATMPDIR)
	$(INSTALL) -m 755 -t $(PMDATMPDIR) Install Remove $(PYSCRIPT) $(PMDAADMDIR)
	@$(INSTALL_MAN)
	$(INSTALL) -m 755 -d $(LOGCONFDIR)
	$(INSTALL) -m 755 -d $(LOGCONFVARDIR)
	$(INSTALL) -m 644 -t $(LOGCONFVARDIR)/summary pmlogconf.summary $(LOGCONFDIR)/summary
	$(INSTALL) -m 755 -d $(PMIECONFDIR)
	$(INSTALL) -m 755 -d $(PMIECONFVARDIR)
	$(INSTALL) -m 644 -t $(PMIECONFVARDIR)/rx_drops openvswitch.rx_drops.pmie $(PMIECONFDIR)/rx_drops
	$(INSTALL) -m 644 -t $(PMIECONFVARDIR)/tx_drops openvswitch.tx_drops.pmie $(PMIECONFDIR)/tx_drops
	$(INSTALL) -m 644 -t $(PMIECONFVARDIR)/errors openvswitch.errors.pmie $(PMIECONFDIR)/errors
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
