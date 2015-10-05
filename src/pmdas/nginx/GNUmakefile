#
# Copyright (c) 2013 Ryan Doyle.
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

IAM	= nginx
DOMAIN	= NGINX
PMDADIR	= $(PCP_PMDAS_DIR)/$(IAM)
LOGCONF	= $(PCP_VAR_DIR)/config/pmlogconf/$(IAM)

MAN_SECTION = 1
MAN_PAGES = pmda$(IAM).$(MAN_SECTION)
MAN_DEST = $(PCP_MAN_DIR)/man$(MAN_SECTION)

LDIRT = domain.h root pmns *.log

default: check_domain

include $(BUILDRULES)

install: default
	$(INSTALL) -m 755 -d $(PMDADIR)
	$(INSTALL) -m 755 Install Remove $(PMDADIR)
	$(INSTALL) -m 644 pmda$(IAM).pl $(PMDADIR)/pmda$(IAM).pl
	$(INSTALL) -m 644 $(IAM).conf $(PMDADIR)/$(IAM).conf
	$(INSTALL) -m 755 -d $(LOGCONF)
	$(INSTALL) -m 644 pmlogconf.summary $(LOGCONF)/summary
	@$(INSTALL_MAN)

default_pcp : default

install_pcp : install

check_domain:	../../pmns/stdpmid
	$(DOMAIN_PERLRULE)
