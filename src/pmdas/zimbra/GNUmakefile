#
# Copyright (c) 2009 Aconex.  All Rights Reserved.
# Copyright (c) 2020 Red Hat.  All Rights Reserved.
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

IAM	= zimbra
LDIRT	= domain.h root pmns *.log
LDIRT	+= runaszimbra

PMDAADMDIR = $(PCP_PMDASADM_DIR)/$(IAM)
PMDATMPDIR = $(PCP_PMDAS_DIR)/$(IAM)
LOGCONFDIR = $(PCP_SYSCONF_DIR)/pmlogconf/$(IAM)
LOGCONFVARDIR = $(PCP_VAR_DIR)/config/pmlogconf/$(IAM)

MAN_SECTION = 1
MAN_PAGES = pmda$(IAM).$(MAN_SECTION)
MAN_DEST = $(PCP_MAN_DIR)/man$(MAN_SECTION)

default: build-me check_domain runaszimbra

include $(BUILDRULES)

ifeq "$(HAVE_PERL)" "true"
build-me:
install: default
	$(INSTALL) -m 755 -d $(PMDAADMDIR)
	$(INSTALL) -m 755 -d $(PMDATMPDIR)
	$(INSTALL) -m 755 -t $(PMDATMPDIR) runaszimbra Install Remove $(PMDAADMDIR)
	$(INSTALL) -m 755 -t $(PMDATMPDIR)/zimbraprobe zimbraprobe.sh $(PMDAADMDIR)/zimbraprobe
	$(INSTALL) -m 644 -t $(PMDATMPDIR)/pmda$(IAM).pl pmda$(IAM).pl $(PMDAADMDIR)/pmda$(IAM).pl
	@$(INSTALL_MAN)
	$(INSTALL) -m 755 -d $(LOGCONFDIR)
	$(INSTALL) -m 755 -d $(LOGCONFVARDIR)
	$(INSTALL) -m 644 -t $(LOGCONFVARDIR)/all pmlogconf.all $(LOGCONFDIR)/all
else
build-me:
install:
endif

default_pcp : default

install_pcp : install

check_domain:	../../pmns/stdpmid
	$(DOMAIN_PERLRULE)

check:: $(MAN_PAGES)
	$(MANLINT) $^
