#
# Copyright (c) 2015-2017 Red Hat.
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

TOPDIR = ../../..
include $(TOPDIR)/src/include/builddefs

CFILES 		= jsmn.c json_helpers.c http_client.c http_parser.c
HFILES		= jsmn.h private.h http_client.h http_parser.h
XFILES		= jsmn.c jsmn.h http_parser.c http_parser.h

LLDLIBS		= -lpcp -lpcp_pmda
LCFLAGS		= -DJSMN_PARENT_LINKS=1 -DHTTP_PARSER_STRICT=0 -DPCP_INTERNAL

STATICLIBTARGET = libpcp_web.a

DSOVERSION = 1
LIBTARGET = libpcp_web.$(DSOSUFFIX).$(DSOVERSION)
SYMTARGET = libpcp_web.$(DSOSUFFIX)

VERSION_SCRIPT = exports
LDIRT = $(XFILES)

default: $(XFILES) $(LIBTARGET) $(SYMTARGET) $(STATICLIBTARGET)

include $(BUILDRULES)

ifneq ($(SYMTARGET),)
$(SYMTARGET):
	$(LN_S) -f $(LIBTARGET) $@
endif

install: default
ifneq ($(LIBTARGET),)
	$(INSTALL) -m 755 $(LIBTARGET) $(PCP_LIB_DIR)/$(LIBTARGET)
endif
ifneq ($(SYMTARGET),)
	for tt in $(SYMTARGET); do \
	    $(INSTALL) -S $(LIBTARGET) $(PCP_LIB_DIR)/$$tt || exit 1; \
	done
endif
ifneq ($(STATICLIBTARGET),)
	$(INSTALL) -m 755 $(STATICLIBTARGET) $(PCP_LIB_DIR)/$(STATICLIBTARGET)
endif

$(XFILES):
	$(LN_S) $(TOPDIR)/src/external/$@ $@

default_pcp:	default

install_pcp:	install

ifneq ($(LIBTARGET),)
$(LIBTARGET): $(VERSION_SCRIPT) $(XFILES)
endif

jsmn.o:		jsmn.c jsmn.h
http_parser.o:	http_parser.c http_parser.h
