#
# Copyright (c) 2018-2020,2022 Red Hat.
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

TOPDIR = ../../..
include $(TOPDIR)/src/include/builddefs

CMDTARGET = pmproxy$(EXECSUFFIX)
HFILES = pmproxy.h
CFILES = pmproxy.c
XFILES = uv_callback.h uv_callback.c
LDIRT = $(XFILES)

LCFLAGS = $(PIECFLAGS) \
	-I$(TOPDIR)/src/libpcp_web/src \
	-I$(TOPDIR)/src/libpcp_web/src/deps \
	-I$(TOPDIR)/src/libpcp_mmv/src
LLDFLAGS = $(PIELDFLAGS)
LLDLIBS	= -lpcp_mmv $(PCP_WEBLIB)
PCPLIB_LDFLAGS += \
	-L$(TOPDIR)/src/libpcp_web/$(LIBPCP_ABIDIR) \
	-L$(TOPDIR)/src/libpcp_mmv/$(LIBPCP_ABIDIR)

ifeq "$(HAVE_LIBUV)" "true"
LCFLAGS += $(LIBUVCFLAGS) -DHAVE_LIBUV=1
SERVLETS = search.c series.c webapi.c
CFILES += openmetrics.c server.c http.c pcp.c uv_callback.c redis.c $(SERVLETS)
HFILES += openmetrics.h server.h http.h pcp.h uv_callback.h
ifeq "$(HAVE_OPENSSL)" "true"
LCFLAGS += $(OPENSSLCFLAGS) -DHAVE_OPENSSL=1
CFILES += secure.c
endif
endif
CFILES += deprecated.c

default:	$(XFILES) $(CMDTARGET)

include $(BUILDRULES)

install:	default
	$(INSTALL) -m 755 $(CMDTARGET) $(PCP_BINADM_DIR)/$(CMDTARGET)

default_pcp:	default

install_pcp:	install

$(XFILES):
	$(LN_S) -f $(TOPDIR)/src/external/$@ $@

$(OBJECTS):	$(HFILES) $(TOPDIR)/src/include/pcp/libpcp.h

check:: $(CFILES) $(HFILES)
	$(CLINT) $^
