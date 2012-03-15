#!gmake
#
# Copyright (c) 2007 Silicon Graphics, Inc.  All Rights Reserved.
# Copyright (c) 2011-2012 Red Hat Inc.
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

LLDLIBS = -lpcp -lmicrohttpd
CFILES	= main.c pmwebapi.c pmresapi.c
CMDTARGET = pmwebapi$(EXECSUFFIX)
LDIRT = $(TARGET)

default:	$(CMDTARGET)

include $(BUILDRULES)

install:	$(CMDTARGET)
	$(INSTALL) -m 755 $(CMDTARGET) $(PCP_BIN_DIR)/$(CMDTARGET)

default_pcp:	default

install_pcp:	install

