#! gmake
#
# Copyright (c) 2006 Nathan Scott.  All Rights Reserved.
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

DSOVERSION = 1
LIBTARGET = libkmtime.$(DSOSUFFIX).$(DSOVERSION)
SYMTARGET = libkmtime.$(DSOSUFFIX)
ifeq "$(TARGET_OS)" "darwin"
LIBTARGET = libkmtime.$(DSOVERSION).$(DSOSUFFIX)
endif
ifeq "$(TARGET_OS)" "cygwin"
LIBTARGET = libkmtime.$(DSOSUFFIX)
SYMTARGET =
endif

CFILES = client.c
HFILES = kmtime.h

default: $(LIBTARGET) $(SYMTARGET)

ifneq "$(SYMTARGET)" ""
$(SYMTARGET):
	$(LN_S) -f $(LIBTARGET) $(SYMTARGET)
endif

include $(BUILDRULES)

$(LIBTARGET) : $(LOBJECTS)

install: default
	$(INSTALL) -m 755 $(LIBTARGET) $(PCP_LIB_DIR)/$(LIBTARGET)
ifneq "$(SYMTARGET)" ""
	$(INSTALL) -S $(LIBTARGET) $(PCP_LIB_DIR)/$(SYMTARGET)
endif
	$(INSTALL) -m 644 kmtime.h $(PCP_INC_DIR)/kmtime.h

default_pcp:	default

install_pcp:	install
