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

TOPDIR = ../../..
include $(TOPDIR)/src/include/builddefs

CMDTARGET = pmcd$(EXECSUFFIX)
HFILES = client.h pmcd.h
CFILES = pmcd.c config.c dofetch.c dopdus.c dostore.c client.c agent.c util.c
LLDLIBS	= -lpcp -lpcp_pmcd $(LIB_FOR_BASENAME) $(LIB_FOR_DLOPEN)

PCP_LIBS += -L$(TOPDIR)/src/libpcp_pmcd/$(LIBPCP_ABIDIR)

# Note: on platforms that need it (see configure.in) the "-rdynamic" flag
# allows the pmcd pmda (dynamicaly loaded shared library) to access the
# global symbols in pmcd.
#LCFLAGS = $(RDYNAMIC_FLAG)
LLDFLAGS = $(RDYNAMIC_FLAG) $(PIELDFLAGS)
LCFLAGS = $(PIECFLAGS)

default:	$(CMDTARGET)

include $(BUILDRULES)

install:	default
	$(INSTALL) -m 755 $(CMDTARGET) $(PCP_BINADM_DIR)/$(CMDTARGET)

default_pcp:	default

install_pcp:	install
