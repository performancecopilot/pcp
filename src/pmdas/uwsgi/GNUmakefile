#
# Copyright (c) 2024 Red Hat.
# Copyright (c) 2024 Nikhil Jain <nikjain@redhat.com>
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

IAM	= uwsgi
DOMAIN	= UWSGI
PYSCRIPT = pmda$(IAM).python

PMDAADMDIR	= $(PCP_PMDASADM_DIR)/$(IAM)
PMDACONFIG	= $(PCP_SYSCONF_DIR)/$(IAM)
PMDATMPDIR	= $(PCP_PMDAS_DIR)/$(IAM)
PMLOGCONFDIR	= $(PCP_SYSCONF_DIR)/pmlogconf/$(IAM)
PMLOGCONFVARDIR	= $(PCP_VAR_DIR)/config/pmlogconf/$(IAM)

LDIRT	= domain.h root pmns *.log

MAN_SECTION	= 1
MAN_PAGES	= pmda$(IAM).$(MAN_SECTION)
MAN_DEST	= $(PCP_MAN_DIR)/man$(MAN_SECTION)

default_pcp default:	build-me

include $(BUILDRULES)

ifeq "$(HAVE_PYTHON)" "true"
build-me:	check_domain
install_pcp install: default
	$(INSTALL) -m 755 -d $(PMDAADMDIR)
	$(INSTALL) -m 755 -d $(PMDATMPDIR)
	$(INSTALL) -m 755 -t $(PMDATMPDIR) Install Remove $(PYSCRIPT) $(PMDAADMDIR)
	$(INSTALL) -m 755 -d $(PMDACONFIG)
	$(INSTALL) -m 600 -t $(PMDATMPDIR)/$(CONF) $(CONF) $(PMDACONFIG)/$(CONF)
	$(INSTALL) -m 755 -d $(PMLOGCONFDIR)
	$(INSTALL) -m 755 -d $(PMLOGCONFVARDIR)
	$(INSTALL) -m 644 -t $(PMLOGCONFVARDIR)/summary pmlogconf.summary $(PMLOGCONFDIR)/summary
	$(INSTALL) -m 644 -t $(PMDATMPDIR)/$(IAM).conf $(IAM).conf $(PMDACONFIG)/$(IAM).conf
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

