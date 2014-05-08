#!gmake
#
# Copyright (c) 2000,2004 Silicon Graphics, Inc.  All Rights Reserved.
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

TOPDIR = ../..
include $(TOPDIR)/src/include/builddefs
-include ./GNUlocaldefs

TARGETS = pmafm mkaf
LSRCFILES = mkaf pmafm pmafm.pcp pmafm.pcp-gui template
AFM_DIR = $(PCP_VAR_DIR)/config/pmafm

default :: default_pcp

default_pcp : $(TARGETS)

default_pro : $(TARGETS)

install :: install_pcp

install_pcp : default_pcp
	$(INSTALL) -m 755 pmafm $(PCP_BIN_DIR)/pmafm$(SHELLSUFFIX)
	$(INSTALL) -m 755 mkaf $(PCP_BINADM_DIR)/mkaf$(SHELLSUFFIX)
	$(INSTALL) -m 755 -d $(AFM_DIR)
	$(INSTALL) -m 644 pmafm.pcp $(AFM_DIR)/pcp
	$(INSTALL) -m 644 pmafm.pcp-gui $(AFM_DIR)/pcp-gui

include $(BUILDRULES)
