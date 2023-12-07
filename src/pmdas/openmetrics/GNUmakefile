#
# Copyright (c) 2017 Ronak Jain.
# Copyright (c) 2017,2019-2020,2023 Red Hat.
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

IAM	= openmetrics
PYSCRIPT = pmda$(IAM).python
LDIRT	= domain.h root pmns $(IAM).log
DOMAIN	= $(IAM)
# common endpoint URLs (on: enabled, off: available)
ONURLS	= grafana.url kepler.url
OFFURLS	= collectd.url etcd.url spark.url vmware.url

MAN_SECTION = 1
MAN_PAGES = pmda$(IAM).$(MAN_SECTION)
MAN_DEST = $(PCP_MAN_DIR)/man$(MAN_SECTION)

PMDAADMDIR = $(PCP_PMDASADM_DIR)/$(IAM)
PMDACONFIG = $(PCP_SYSCONF_DIR)/$(IAM)
PMDATMPDIR = $(PCP_PMDAS_DIR)/$(IAM)
LOGCONFDIR = $(PCP_SYSCONF_DIR)/pmlogconf/$(IAM)
LOGCONFVARDIR = $(PCP_VAR_DIR)/config/pmlogconf/$(IAM)

default_pcp default ::	build-me

include $(BUILDRULES)

ifeq "$(PMDA_OPENMETRICS)" "true"
build-me:	check_domain

install_pcp install :: default
	$(INSTALL) -m 755 -d $(PMDAADMDIR)
	$(INSTALL) -m 755 -d $(PMDATMPDIR)
	$(INSTALL) -m 755 -d $(PMDATMPDIR)/config.d
	$(INSTALL) -m 755 -d $(PMDACONFIG)
	@for url in $(ONURLS); do \
	    $(INSTALL) -m 644 -t $(PMDATMPDIR)/config.d/$$url $$url $(PMDACONFIG)/$$url; \
	done
	@for url in $(OFFURLS); do \
	    $(INSTALL) -m 644 $$url $(PMDACONFIG)/$$url; \
	done
	$(INSTALL) -m 755 -t $(PMDATMPDIR) Install Remove Upgrade $(PYSCRIPT) $(PMDAADMDIR)
	$(INSTALL) -m 755 -d $(PCP_VAR_DIR)/config/pmlogconf/$(IAM)
	$(INSTALL) -m 755 -d $(LOGCONFDIR)
	$(INSTALL) -m 755 -d $(LOGCONFVARDIR)
	$(INSTALL) -m 644 -t $(LOGCONFVARDIR)/summary pmlogconf.summary $(LOGCONFDIR)/summary
	@$(INSTALL_MAN)

check:: $(PYSCRIPT)
	$(PYLINT) $^

else
build-me:
install_pcp install:
endif

check_domain:	../../pmns/stdpmid
	$(DOMAIN_PYTHONRULE)

check:: $(MAN_PAGES)
	$(MANLINT) $^
