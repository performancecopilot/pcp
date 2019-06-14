#
# Copyright (c) 2018-2019 Red Hat.
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

LCFLAGS = $(PIECFLAGS) -I$(TOPDIR)/src/libpcp_web/src
LLDFLAGS = $(PIELDFLAGS)
LLDLIBS	= $(PCP_WEBLIB)

ifeq "$(HAVE_LIBUV) and $(HAVE_OPENSSL)" "true and true"
LCFLAGS += $(LIBUVCFLAGS) -DHAVE_LIBUV=1 $(OPENSSLCFLAGS) -DHAVE_OPENSSL=1
LDFLAGS += $(LIB_FOR_OPENSSL)
SERVLETS = series.c webapi.c
CFILES += openmetrics.c server.c http.c pcp.c redis.c secure.c $(SERVLETS)
HFILES += openmetrics.h server.h http.h pcp.h
endif
CFILES += deprecated.c

default:	$(CMDTARGET)

include $(BUILDRULES)

install:	default
	$(INSTALL) -m 755 $(CMDTARGET) $(PCP_BINADM_DIR)/$(CMDTARGET)

default_pcp:	default

install_pcp:	install

$(OBJECTS):	$(HFILES) $(TOPDIR)/src/include/pcp/libpcp.h

check:: $(CFILES) $(HFILES)
	$(CLINT) $^
