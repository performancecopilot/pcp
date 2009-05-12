#
# Copyright (c) 2006 Aconex.  All Rights Reserved.
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

TOPDIR = ../../..
include	$(TOPDIR)/src/include/builddefs

CFILES = timeclient.c timestate.c record.c
LLDLIBS = -lpcp

STATICLIBTARGET = libpcp_gui.a

DSOVERSION = 2
LIBTARGET = libpcp_gui.$(DSOSUFFIX).$(DSOVERSION)
SYMTARGET = libpcp_gui.$(DSOSUFFIX) libpcp_gui.$(DSOSUFFIX).1

ifeq "$(PACKAGE_DISTRIBUTION)" "debian"
SYMTARGET = libpcp_gui.$(DSOSUFFIX)
endif
ifeq "$(TARGET_OS)" "darwin"
LIBTARGET = libpcp_gui.$(DSOVERSION).$(DSOSUFFIX)
endif
ifneq "$(findstring $(TARGET_OS),cygwin mingw)" ""
LIBTARGET = libpcp_gui.$(DSOSUFFIX)
STATICLIBTARGET =
SYMTARGET =
endif

LDIRT = $(SYMTARGET)

default: $(LIBTARGET) $(SYMTARGET) $(STATICLIBTARGET)

ifneq "$(SYMTARGET)" ""
$(SYMTARGET):
	$(LN_S) -f $(LIBTARGET) $@
endif

include $(BUILDRULES)

install: default
	$(INSTALL) -m 755 $(LIBTARGET) $(PCP_LIB_DIR)/$(LIBTARGET)
ifneq "$(SYMTARGET)" ""
	for tt in $(SYMTARGET); do \
	    $(INSTALL) -S $(LIBTARGET) $(PCP_LIB_DIR)/$$tt || exit 1; \
	done
endif
ifneq "$(STATICLIBTARGET)" ""
	$(INSTALL) -m 755 $(STATICLIBTARGET) $(PCP_LIB_DIR)/$(STATICLIBTARGET)
endif

default_pcp:	default

install_pcp:	install
