#
# Copyright (c) 2001,2009 Silicon Graphics, Inc.  All Rights Reserved.
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


TOPDIR = ../../..
include $(TOPDIR)/src/include/builddefs
-include ./GNUlocaldefs

CFILES	= mmv_stats.c

STATICLIBTARGET	= libpcp_mmv.a
DSOVERSION = 1
ifeq "$(TARGET_OS)" "darwin"
LIBTARGET = libpcp_mmv.$(DSOVERSION).$(DSOSUFFIX)
else
LIBTARGET = libpcp_mmv.$(DSOSUFFIX).$(DSOVERSION)
endif
SYMTARGET = libpcp_mmv.$(DSOSUFFIX) 

LCFLAGS = -I.
LDIRT = $(SYMTARGET) pcp

base default: pcp $(LIBTARGET) $(SYMTARGET) $(STATICLIBTARGET)

include $(BUILDRULES)

install: default
	$(INSTALL) -m 755 $(LIBTARGET) $(PCP_LIB_DIR)/$(LIBTARGET)
	for tt in $(SYMTARGET); do \
		$(INSTALL) -S $(LIBTARGET) $(PCP_LIB_DIR)/$$tt || exit 1; \
	done
	$(INSTALL) -m 755 $(STATICLIBTARGET) $(PCP_LIB_DIR)/$(STATICLIBTARGET)

default_pcp: default

install_pcp: install

pcp:
	$(LN_S) $(TOPDIR)/src/include pcp

ifneq "$(SYMTARGET)" ""
$(SYMTARGET):
	$(LN_S) -f $(LIBTARGET) $@
endif

