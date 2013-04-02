#
# Copyright (c) 2008 Aconex.  All Rights Reserved.
# Copyright (c) 2000,2003,2004 Silicon Graphics, Inc.  All Rights Reserved.
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
# Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
#

TOPDIR = ../../..
include $(TOPDIR)/src/include/builddefs
-include ./GNUlocaldefs

CFILES = data.c trace.c client.c

LCFLAGS = -I$(TOPDIR)/src/pmcd/src -I$(TOPDIR)/src/libpcp/src -DPMCD_INTERNAL
LLDLIBS = -lpcp

ifeq "$(ENABLE_SECURE)" "true"
LCFLAGS += $(NSSCFLAGS) $(NSPRCFLAGS)
endif

ifeq "$(TARGET_OS)" "mingw"
LIBTARGET = libpcp_pmcd.$(DSOSUFFIX)
LDIRT	+= libpcp_pmcd.a
else
STATICLIBTARGET = libpcp_pmcd.a
LDIRT	+= libpcp_pmcd.$(DSOSUFFIX)
endif

default : $(LIBTARGET) $(STATICLIBTARGET)

include $(BUILDRULES)

install : default
ifeq "$(TARGET_OS)" "mingw"
	$(INSTALL) -m 755 $(LIBTARGET) $(PCP_LIB_DIR)/$(LIBTARGET)
endif

default_pcp : default

install_pcp : install
