#
# Copyright (c) 2018-2019 Red Hat.
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

IAM	= elasticsearch
DOMAIN	= ELASTICSEARCH
PYSCRIPT = pmda$(IAM).python
PMDADIR	= $(PCP_PMDAS_DIR)/$(IAM)
LOGCONF	= $(PCP_VAR_DIR)/config/pmlogconf/$(IAM)

LDIRT	= domain.h root pmns *.log

MAN_SECTION	= 1
MAN_PAGES	= pmda$(IAM).$(MAN_SECTION)
MAN_DEST	= $(PCP_MAN_DIR)/man$(MAN_SECTION)

default_pcp default:	build-me

include $(BUILDRULES)

ifeq "$(PMDA_ELASTICSEARCH)" "true"
build-me:	check_domain
install_pcp install: default
	$(INSTALL) -m 755 -d $(PMDADIR)
	$(INSTALL) -m 755 Install Remove Upgrade $(PYSCRIPT) $(PMDADIR)
	$(INSTALL) -m 755 -d $(LOGCONF)
	$(INSTALL) -m 644 pmlogconf.summary $(LOGCONF)/summary
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
