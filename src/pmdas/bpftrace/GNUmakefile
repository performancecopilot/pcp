#
# Copyright (c) 2019-2020,2022 Red Hat.
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

IAM      = bpftrace
PYSCRIPT = pmda$(IAM).python
LDIRT    = domain.h root pmns $(IAM).log
CONFIG   = bpftrace.conf

DOMAIN   = BPFTRACE
SUBDIRS  = bpftrace autostart examples

PMDAADMDIR = $(PCP_PMDASADM_DIR)/$(IAM)
PMDACONFIG = $(PCP_SYSCONF_DIR)/$(IAM)
PMDATMPDIR = $(PCP_PMDAS_DIR)/$(IAM)

MAN_SECTION = 1
MAN_PAGES = pmda$(IAM).$(MAN_SECTION)
MAN_DEST = $(PCP_MAN_DIR)/man$(MAN_SECTION)

default_pcp default ::	build-me

include $(BUILDRULES)

ifeq "$(PMDA_BPFTRACE)" "true"
build-me:	check_domain

install_pcp install ::	default
	$(INSTALL) -m 755 -d $(PMDAADMDIR)
	$(INSTALL) -m 755 -d $(PMDATMPDIR)
	$(INSTALL) -m 755 -t $(PMDATMPDIR) Install Remove Upgrade $(PYSCRIPT) $(PMDAADMDIR)
	$(INSTALL) -m 644 -t $(PMDATMPDIR)/README.md README.md $(PMDAADMDIR)/README.md
	$(INSTALL) -m 755 -d $(PMDACONFIG)
	$(INSTALL) -m 644 -t $(PMDATMPDIR)/$(CONFIG) $(CONFIG) $(PMDACONFIG)/$(CONFIG)
	@$(INSTALL_MAN)

default_pcp default ::	$(SUBDIRS)
	$(SUBDIRS_MAKERULE)

install_pcp install ::	$(SUBDIRS)
	$(SUBDIRS_MAKERULE)
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
