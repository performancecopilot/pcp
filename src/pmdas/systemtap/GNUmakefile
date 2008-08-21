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
# 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
# 

TOPDIR = ../../..
include $(TOPDIR)/src/include/builddefs

IAM	= systemtap
DOMAIN	= SYSTEMTAP
PMDADIR	= $(PCP_PMDAS_DIR)/$(IAM)
LSRCFILES = Install Remove pmdasystemtap.pl probes.stp README

default: 

include $(BUILDRULES)

install: default
ifeq "$(TARGET_OS)" "linux"
	$(INSTALL) -m 755 -d $(PMDADIR)
	$(INSTALL) -m 644 probes.stp $(PMDADIR)/probes.stp
	$(INSTALL) -m 755 Install Remove pmdasystemtap.pl $(PMDADIR)
endif

default_pcp : default

install_pcp : install
