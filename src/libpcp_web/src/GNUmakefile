#
# Copyright (c) 2015-2018 Red Hat.
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

CFILES = jsmn.c http_client.c http_parser.c sds.c siphash.c \
	 query.c schema.c load.c crc16.c sha1.c util.c slots.c \
	 redis.c net.c dict.c maps.c \
	 json_helpers.c
HFILES = jsmn.h http_client.h http_parser.h sdsalloc.h zmalloc.h \
	 query.h schema.h load.h crc16.h sha1.h util.h slots.h \
	 redis.h net.h dict.h maps.h \
	 private.h libuv.h
YFILES = query_parser.y
XFILES = jsmn.c jsmn.h http_parser.c http_parser.h crc16.c crc16.h \
	 sha1.c sha1.h sds.c siphash.c dict.c dict.h

LLDLIBS = $(PCPLIB) $(PCP_PMDALIB) $(LIB_FOR_MATH)
ifeq "$(TARGET_OS)" "mingw"
LLDLIBS += -lws2_32
endif

LDIRT = $(XFILES) $(YFILES:%.y=%.tab.?)
LCFLAGS += -DJSMN_PARENT_LINKS=1 -DJSMN_STRICT=1 -DHTTP_PARSER_STRICT=0

ifeq "$(HAVE_LIBUV)" "true"
CFILES += libuv.c discover.c
HFILES += discover.h
LCFLAGS += $(LIBUVCFLAGS) -DHAVE_LIBUV=1
LLDLIBS += $(LIB_FOR_LIBUV)
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
	$(LN_S) -f $(TOPDIR)/src/external/$@ $@

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

http_client.o load.o query.o query_parser.o:	$(TOPDIR)/src/include/pcp/libpcp.h
