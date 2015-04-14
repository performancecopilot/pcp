#
# Copyright (c) 2015 Martins Innus
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

IAM	= mic
PYSCRIPT = pmda$(IAM).python
LDIRT	= domain.h root pmns $(IAM).log

MAN_SECTION = 1
MAN_PAGES = pmda$(IAM).$(MAN_SECTION)
MAN_DEST = $(PCP_MAN_DIR)/man$(MAN_SECTION)

DOMAIN	= MIC
PMDADIR	= $(PCP_PMDAS_DIR)/$(IAM)

default_pcp default:	check_domain

include $(BUILDRULES)

install_pcp install:	default 
	$(INSTALL) -m 755 -d $(PMDADIR)
	$(INSTALL) -m 755 Install Remove $(PMDADIR)
	$(INSTALL) -m 644 $(PYSCRIPT) $(PMDADIR)/$(PYSCRIPT)
	@$(INSTALL_MAN)

check_domain:	../../pmns/stdpmid
	$(DOMAIN_PYTHONRULE)
