#!gmake
#
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
# You should have received a copy of the GNU General Public License along
# with this program; if not, write to the Free Software Foundation, Inc.,
# 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
# 

TOPDIR = ../../..
include $(TOPDIR)/src/include/builddefs

IAM	= snmp
PMDADIR	= $(PCP_PMDAS_DIR)/$(IAM)
LSRCFILES = Install Remove pmda$(IAM).pl snmp.conf
LDIRT	= domain.h root pmns *.log $(MAN_PAGES)

ifneq ($(POD2MAN),)
MAN_SECTION = 1
MAN_PAGES = pmda$(IAM).$(MAN_SECTION)
MAN_DEST = $(PCP_MAN_DIR)/man$(MAN_SECTION)
endif

default_pcp default:	build-me $(MAN_PAGES)

pmda$(IAM).1: pmda$(IAM).pl
	$(POD_MAKERULE)

include $(BUILDRULES)

ifeq "$(PMDA_SNMP)" "true"
build-me:	check_domain
install_pcp install: default
	$(INSTALL) -m 755 -d $(PMDADIR)
	$(INSTALL) -m 755 Install Remove $(PMDADIR)
	$(INSTALL) -m 644 pmda$(IAM).pl $(PMDADIR)/pmda$(IAM).pl
	$(INSTALL) -m 644 snmp.conf $(PMDADIR)/snmp.conf
	@$(INSTALL_MAN)
else
build-me:
install_pcp install: default
	@$(INSTALL_MAN)
endif

check_domain:	../../pmns/stdpmid
	$(DOMAIN_PERLRULE)
