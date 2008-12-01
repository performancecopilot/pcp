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
# 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
#

TOPDIR = ../..
include $(TOPDIR)/src/include/builddefs

CFILES = pmpost.c
CMDTARGET = pmpost$(EXECSUFFIX)
LLDLIBS = -lpcp $(LIB_FOR_BASENAME)

default:	build-me

ifneq "$(TARGET_OS)" "mingw"
build-me:	$(CMDTARGET)

install:	default
	$(INSTALL) -m 4755 -o root $(CMDTARGET) $(PCP_BINADM_DIR)/$(CMDTARGET)
else
build-me:
install:
endif

include $(BUILDRULES)

default_pcp:	default

install_pcp:	install
