#
# Copyright (c) 2017 Red Hat.
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

IAM = nutcracker
DOMAIN = NUTCRACKER
PMDADIR	= $(PCP_PMDAS_DIR)/$(IAM)
LOGCONF	= $(PCP_VAR_DIR)/config/pmlogconf/$(IAM)

LDIRT = domain.h root pmns *.log

MAN_SECTION = 1
MAN_PAGES = pmda$(IAM).$(MAN_SECTION)
MAN_DEST = $(PCP_MAN_DIR)/man$(MAN_SECTION)

default_pcp default:	build-me $(MAN_PAGES)

include $(BUILDRULES)

ifeq "$(PMDA_NUTCRACKER)" "true"
build-me: check_domain
install_pcp install: default
	$(INSTALL) -m 755 -d $(PMDADIR)
	$(INSTALL) -m 755 Install Remove $(PMDADIR)
	$(INSTALL) -m 644 pmda$(IAM).pl $(PMDADIR)/pmda$(IAM).pl
	$(INSTALL) -m 644 nutcracker.conf $(PMDADIR)/nutcracker.conf
	@$(INSTALL_MAN)
else
build-me:
install_pcp install: default
	@$(INSTALL_MAN)
endif

check_domain:	../../pmns/stdpmid
	$(DOMAIN_PERLRULE)
