#
# Copyright (c) 2014-2015 Red Hat.
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

IAM	= json
PYSCRIPT = pmda$(IAM).python
LSRCFILES = Install Remove $(PYSCRIPT)

DOMAIN	= JSON
PMDADIR	= $(PCP_PMDAS_DIR)/$(IAM)

MAN_SECTION = 1
MAN_PAGES = pmda$(IAM).$(MAN_SECTION)
MAN_DEST = $(PCP_MAN_DIR)/man$(MAN_SECTION)

LDIRT	= domain.h $(IAM).log

default_pcp default:	domain.h check_domain

include $(BUILDRULES)

install_pcp install:	default 
	$(INSTALL) -m 755 -d $(PMDADIR)
	$(INSTALL) -m 755 Install Remove generate_ceph_metadata $(PMDADIR)
	$(INSTALL) -m 644 domain.h config.json README $(PYSCRIPT) $(PMDADIR)
	@$(INSTALL_MAN)

domain.h: ../../pmns/stdpmid
	$(DOMAIN_MAKERULE)

check_domain:	../../pmns/stdpmid
	$(DOMAIN_PYTHONRULE)
