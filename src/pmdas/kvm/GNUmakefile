#!gmake
#
# Copyright (c) 2008 Aconex.  All Rights Reserved.
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
# You should have received a copy of the GNU General Public License along
# with this program; if not, write to the Free Software Foundation, Inc.,
# 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
# 

TOPDIR = ../../..
include $(TOPDIR)/src/include/builddefs

IAM	= kvm
PMDADIR	= $(PCP_PMDAS_DIR)/$(IAM)
LSRCFILES = Install Remove pmda$(IAM).pl
LDIRT	= domain.h root pmns *.log $(MAN_PAGES)

ifneq ($(POD2MAN),)
MAN_SECTION = 1
MAN_PAGES = pmda$(IAM).$(MAN_SECTION)
MAN_DEST = $(PCP_MAN_DIR)/man$(MAN_SECTION)
endif

default: check_domain $(MAN_PAGES)

pmda$(IAM).1: pmda$(IAM).pl
	$(POD_MAKERULE)

include $(BUILDRULES)

ifeq "$(TARGET_OS)" "linux"
install: default
	$(INSTALL) -m 755 -d $(PMDADIR)
	$(INSTALL) -m 755 Install Remove $(PMDADIR)
	$(INSTALL) -m 644 pmda$(IAM).pl $(PMDADIR)/pmda$(IAM).pl
	@$(INSTALL_MAN)
else
install:
endif

default_pcp : default

install_pcp : install

check_domain:	../../pmns/stdpmid
	$(DOMAIN_PERLRULE)
