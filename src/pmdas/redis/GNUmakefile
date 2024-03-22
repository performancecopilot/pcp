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

IAM = redis
DOMAIN = REDIS

PMDATMPDIR = $(PCP_PMDAS_DIR)/$(IAM)
PMDACONFIG = $(PCP_SYSCONF_DIR)/$(IAM)
PMDAADMDIR = $(PCP_PMDASADM_DIR)/$(IAM)
LOGCONFDIR = $(PCP_SYSCONF_DIR)/pmlogconf/$(IAM)
LOGCONFVARDIR = $(PCP_VAR_DIR)/config/pmlogconf/$(IAM)

LDIRT = domain.h root pmns *.log

MAN_SECTION = 1
MAN_PAGES = pmda$(IAM).$(MAN_SECTION)
MAN_DEST = $(PCP_MAN_DIR)/man$(MAN_SECTION)

default: build-me check_domain

include $(BUILDRULES)

ifeq "$(HAVE_PERL)" "true"
build-me: $(MAN_PAGES)
install: default
	$(INSTALL) -m 755 -d $(PMDAADMDIR)
	$(INSTALL) -m 755 -d $(PMDATMPDIR)
	$(INSTALL) -m 755 -t $(PMDATMPDIR) Install Remove $(PMDAADMDIR)
	$(INSTALL) -m 644 -t $(PMDATMPDIR)/pmda$(IAM).pl pmda$(IAM).pl $(PMDAADMDIR)/pmda$(IAM).pl
	$(INSTALL) -m 755 -d $(PMDACONFIG)
	$(INSTALL) -m 644 -t $(PMDATMPDIR)/redis.conf redis.conf $(PMDACONFIG)/redis.conf
	@$(INSTALL_MAN)
	$(INSTALL) -m 755 $(IAM).pmcheck $(PCP_SHARE_DIR)/lib/pmcheck/pmda-$(IAM)
else
build-me:
install: $(TOPDIR)/src/pmcheck/pmda.na.template
	$(INSTALL) -m 755 $< $(PCP_SHARE_DIR)/lib/pmcheck/pmda-$(IAM)
endif

default_pcp : default

install_pcp : install

check_domain:	../../pmns/stdpmid
	$(DOMAIN_PERLRULE)

check:: $(MAN_PAGES)
	$(MANLINT) $^
