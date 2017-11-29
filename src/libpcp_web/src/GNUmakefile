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

CFILES = jsmn.c http_client.c http_parser.c json_helpers.c
HFILES = jsmn.h http_client.h http_parser.h private.h
XFILES = jsmn.c jsmn.h http_parser.c http_parser.h

LLDLIBS = $(PCPLIB) $(PCP_PMDALIB)
LDIRT = $(XFILES) $(YFILES:%.y=%.tab.?)
LCFLAGS += -DJSMN_PARENT_LINKS=1 -DHTTP_PARSER_STRICT=0

ifeq "$(HAVE_HIREDIS)" "true"
CFILES += query.c redis.c load.c sha1.c util.c
HFILES += query.h redis.h load.h sha1.h util.h
YFILES += query_parser.y
XFILES += sha1.c sha1.h
LLDLIBS += $(LIB_FOR_HIREDIS) $(LIB_FOR_MATH)
else
CFILES += noseries.c
endif

STATICLIBTARGET = libpcp_web.a

DSOVERSION = 1
LIBTARGET = libpcp_web.$(DSOSUFFIX).$(DSOVERSION)
SYMTARGET = libpcp_web.$(DSOSUFFIX)

VERSION_SCRIPT = exports
LDIRT = $(XFILES) $(YFILES:%.y=%.tab.?)

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

.NOTPARALLEL:
query_parser.tab.h query_parser.tab.c: query_parser.y query.h
	$(YACC) -d -b `basename $< .y` $<

default_pcp:	default

install_pcp:	install

ifneq ($(LIBTARGET),)
$(LIBTARGET): $(VERSION_SCRIPT) $(XFILES)
endif

jsmn.o:		jsmn.c jsmn.h
http_parser.o:	http_parser.c http_parser.h
