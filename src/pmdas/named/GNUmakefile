#
# Copyright (c) 2009 Aconex.  All Rights Reserved.
# Copyright (c) 2020 Red Hat.
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

IAM	= named
LDIRT	= domain.h root pmns *.log $(MAN_PAGES)

PMDAADMDIR = $(PCP_PMDASADM_DIR)/$(IAM)
PMDATMPDIR = $(PCP_PMDAS_DIR)/$(IAM)

ifneq ($(POD2MAN),)
MAN_SECTION = 1
MAN_PAGES = pmda$(IAM).$(MAN_SECTION)
MAN_DEST = $(PCP_MAN_DIR)/man$(MAN_SECTION)
endif

default: build-me check_domain

include $(BUILDRULES)

default_pcp : default
install_pcp : install

ifneq ($(POD2MAN),)
pmda$(IAM).1: pmda$(IAM).pl
	$(POD_MAKERULE)
endif

ifeq "$(HAVE_PERL)" "true"
build-me: $(MAN_PAGES)
install: default
	$(INSTALL) -m 755 -d $(PMDAADMDIR)
	$(INSTALL) -m 755 -d $(PMDATMPDIR)
	$(INSTALL) -m 755 -t $(PMDATMPDIR) Install Remove $(PMDAADMDIR)
	$(INSTALL) -m 644 -t $(PMDATMPDIR)/pmda$(IAM).pl pmda$(IAM).pl $(PMDAADMDIR)/pmda$(IAM).pl
	@$(INSTALL_MAN)

check_domain:	../../pmns/stdpmid
	$(DOMAIN_PERLRULE)
else
build-me:
install:
check_domain:
endif

check:: $(MAN_PAGES)
	$(MANLINT) $^
