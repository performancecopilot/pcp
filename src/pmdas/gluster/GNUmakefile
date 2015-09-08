#
# Copyright (c) 2013-2014 Red Hat.
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

IAM	= gluster
PYSCRIPT = pmda$(IAM).python
LDIRT	= domain.h root pmns $(IAM).log

DOMAIN	= GLUSTER
PMDADIR	= $(PCP_PMDAS_DIR)/$(IAM)

MAN_SECTION = 1
MAN_PAGES = pmda$(IAM).$(MAN_SECTION)
MAN_DEST = $(PCP_MAN_DIR)/man$(MAN_SECTION)

default_pcp default:	check_domain

include $(BUILDRULES)

install_pcp install:	default 
	$(INSTALL) -m 755 -d $(PMDADIR)
	$(INSTALL) -m 755 Install Remove $(PYSCRIPT) $(PMDADIR)
	@$(INSTALL_MAN)

check_domain:	../../pmns/stdpmid
	$(DOMAIN_PYTHONRULE)
