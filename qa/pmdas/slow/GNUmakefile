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

IAM	= slow
TESTDIR = $(PCP_VAR_DIR)/testsuite/pmdas/$(IAM)
LSRCFILES = Install Remove pmda$(IAM).pl GNUmakefile.install
LDIRT	= domain.h root pmns *.log

default default_pcp setup: check_domain

include $(BUILDRULES)

install: default
	$(INSTALL) -m 755 -d $(TESTDIR)
	$(INSTALL) -m 755 Install Remove $(TESTDIR)
	$(INSTALL) -m 644 pmda$(IAM).pl $(TESTDIR)/pmda$(IAM).pl
	$(INSTALL) -m 644 GNUmakefile.install $(TESTDIR)/GNUmakefile

install_pcp: install

check_domain:	$(TOPDIR)/src/pmns/stdpmid
	$(DOMAIN_PERLRULE)
