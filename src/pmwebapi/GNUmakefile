#!gmake
#
# Copyright (c) 2011-2013 Red Hat Inc.
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

ifeq "$(HAVE_LIBMICROHTTPD)" "1"

LLDLIBS = -lpcp -lmicrohttpd $(LIB_FOR_DLOPEN) $(LIB_FOR_PTHREADS) $(LIB_FOR_MATH) $(LIB_FOR_RT)
LCFLAGS += -Wextra
CFILES	= main.c pmwebapi.c pmresapi.c
CMDTARGET = pmwebapi$(EXECSUFFIX)
LDIRT = $(TARGET)

default:	$(CMDTARGET)

$(CMDTARGET): $(BUNDLEDLIB)

install:	$(CMDTARGET)
	$(INSTALL) -m 755 $(CMDTARGET) $(PCP_BIN_DIR)/$(CMDTARGET)

default_pcp:	default
install_pcp:	install

else

# no libmicrohttpd => no pmwebd

default:
install:
default_pcp:
install_pcp:

endif

include $(BUILDRULES)
