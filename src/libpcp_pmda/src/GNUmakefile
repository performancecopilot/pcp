#! gmake
#
# Copyright (c) 2000,2004 Silicon Graphics, Inc.  All Rights Reserved.
# 
# This library is free software; you can redistribute it and/or modify it
# under the terms of the GNU Lesser General Public License as published
# by the Free Software Foundation; either version 2.1 of the License, or
# (at your option) any later version.
# 
# This library is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
# or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
# License for more details.
# 
# You should have received a copy of the GNU Lesser General Public License
# along with this library; if not, write to the Free Software Foundation,
# Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA.
# 
# Contact information: Silicon Graphics, Inc., 1500 Crittenden Lane,
# Mountain View, CA 94043, USA, or: http://www.sgi.com
#
# $Id: GNUmakefile,v 2.33 2007/02/20 00:08:32 kimbrr Exp $

TOPDIR = ../../..
include $(TOPDIR)/src/include/builddefs
-include GNUlocaldefs

#
# libpcp.so   -> libpcp.so.3
# libpcp.so.2 -> libpcp.so.3
#
DSOVERSION_V2 = 2
DSOVERSION_V3 = 3

ifneq "$(TARGET_OS)" "darwin"
LIBTARGET_V1 = libpcp_pmda.$(DSOSUFFIX)
LIBTARGET_V2 = libpcp_pmda.$(DSOSUFFIX).$(DSOVERSION_V2)
LIBTARGET_V3 = libpcp_pmda.$(DSOSUFFIX).$(DSOVERSION_V3)
else
LIBTARGET_V1 = libpcp_pmda.$(DSOSUFFIX)
LIBTARGET_V2 = 
LIBTARGET_V3 = libpcp_pmda.$(DSOVERSION_V3).$(DSOSUFFIX)
endif
ifeq "$(PACKAGE_DISTRIBUTION)" "debian"
LIBTARGET_V2 = 
endif
LIBTARGET = $(LIBTARGET_V3)

ifeq "$(TARGET_OS)" "cygwin"
LIBTARGET = libpcp_pmda.$(DSOSUFFIX)
LIBTARGET_V1 =
LIBTARGET_V2 =
LIBTARGET_V3 =
endif

STATICLIBTARGET = libpcp_pmda.a

CFILES	= callback.c open.c mainloop.c help.c cache.c
HFILES = libdefs.h
LLDLIBS	= -lpcp

LDIRT = $(LIBTARGET_V1) $(LIBTARGET_V2) $(LIBTARGET_V3)

default: $(LIBTARGET_V1) $(LIBTARGET_V2) $(LIBTARGET) $(STATICLIBTARGET)

$(LIBTARGET_V1): $(LIBTARGET_V3)
	$(LN_S) -f $(LIBTARGET_V3) $(LIBTARGET_V1)

$(LIBTARGET_V2): $(LIBTARGET_V3)
	$(LN_S) -f $(LIBTARGET_V3) $(LIBTARGET_V2)

include $(BUILDRULES)

install : default
ifneq "$(LIBTARGET_V1)" ""
	$(INSTALL) -S $(LIBTARGET_V3) $(PCP_LIB_DIR)/$(LIBTARGET_V1)
endif
ifneq "$(LIBTARGET_V2)" ""
	$(INSTALL) -S $(LIBTARGET) $(PCP_LIB_DIR)/$(LIBTARGET_V2)
endif
	$(INSTALL) -m 755 $(LIBTARGET) $(PCP_LIB_DIR)/$(LIBTARGET)
	$(INSTALL) -m 755 $(STATICLIBTARGET) $(PCP_LIB_DIR)/$(STATICLIBTARGET)

default_pcp : default

install_pcp : install

