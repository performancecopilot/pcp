#
# Copyright (c) 2015-2021 Red Hat.
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

REWRITEDIR	= $(PCP_SYSCONF_DIR)/pmlogrewrite
REWRITEVARDIR	= $(PCP_VAR_DIR)/config/pmlogrewrite

ifneq "$(HAVE_LIBINIH)" "true"
INIH_HFILES = $(addprefix deps/inih/, ini.h)
INIH_CFILES = $(addprefix deps/inih/, ini.c)
INIH_XFILES = $(INIH_HFILES) $(INIH_CFILES)
else
INIH_XFILES =
endif

LIBVALKEY_HFILES = $(addprefix deps/libvalkey/, \
	include/valkey/alloc.h include/valkey/async.h src/async_private.h src/fmacros.h include/valkey/valkey.h include/valkey/net.h \
	include/valkey/read.h include/valkey/visibility.h src/sds.h src/sdsalloc.h include/valkey/sockcompat.h src/win32.h include/valkey/adapters/libuv.h \
	src/adlist.h src/command.h src/vkutil.h include/valkey/cluster.h src/valkey_private.h src/cmddef.h)
LIBVALKEY_CFILES = $(addprefix deps/libvalkey/, \
	src/alloc.c src/async.c src/valkey.c src/net.c src/sds.c src/sockcompat.c src/read.c \
	src/adlist.c src/command.c src/crc16.c src/vkutil.c src/cluster.c src/conn.c)
LIBVALKEY_XFILES = $(LIBVALKEY_HFILES) $(LIBVALKEY_CFILES)

CFILES = jsmn.c http_client.c http_parser.c siphash.c \
	 query.c schema.c load.c sha1.c util.c slots.c \
	 keys.c dict.c maps.c batons.c encoding.c \
	 search.c json_helpers.c config.c \
	 $(LIBVALKEY_CFILES)
ifneq "$(HAVE_LIBINIH)" "true"
	 CFILES += $(INIH_CFILES)
endif
HFILES = jsmn.h http_client.h http_parser.h zmalloc.h \
	 query.h schema.h load.h sha1.h util.h slots.h \
	 keys.h dict.h maps.h batons.h encoding.h \
	 search.h discover.h private.h \
	 $(LIBVALKEY_HFILES)
ifneq "$(HAVE_LIBINIH)" "true"
	 HFILES += $(INIH_HFILES)
endif
YFILES = query_parser.y
XFILES = jsmn.c jsmn.h http_parser.c http_parser.h \
	 sha1.c sha1.h siphash.c dict.c dict.h

LLDLIBS = $(PCPWEBLIB_EXTRAS) $(LIB_FOR_MATH) $(LIB_FOR_REGEX)
ifeq "$(TARGET_OS)" "mingw"
LLDLIBS += -lws2_32
CFILES += fnmatch.c
HFILES += fnmatch.h
XFILES += fnmatch.c fnmatch.h
LCFLAGS += -I.
endif
ifneq "$(HAVE_LIBINIH)" "true"
LCFLAGS += -Iinih
endif

LCFLAGS += $(C99_CFLAGS) -DJSMN_PARENT_LINKS=1 -DJSMN_STRICT=1 -DHTTP_PARSER_STRICT=0 -Ideps -Ideps/libvalkey/include/valkey

ifeq "$(HAVE_LIBUV)" "true"
CFILES += discover.c loggroup.c webgroup.c timer.c
LCFLAGS += $(LIBUVCFLAGS) -DHAVE_LIBUV=1
LLDLIBS += $(LIB_FOR_LIBUV)
else
CFILES += nodiscover.c nologgroup.c nowebgroup.c notimer.c
endif

ifeq "$(HAVE_LIBINIH)" "true"
LLDLIBS += $(LIB_FOR_INIH)
endif

ifeq "$(HAVE_OPENSSL)" "true"
LCFLAGS += $(LIBOPENSSLCFLAGS) -DHAVE_OPENSSL=1
LLDLIBS += $(LIB_FOR_OPENSSL)
endif

STATICLIBTARGET = libpcp_web.a

DSOVERSION = 1
LIBTARGET = libpcp_web.$(DSOSUFFIX).$(DSOVERSION)
SYMTARGET = libpcp_web.$(DSOSUFFIX)

ifeq "$(TARGET_OS)" "darwin"
LIBTARGET = libpcp_web.$(DSOVERSION).$(DSOSUFFIX)
SYMTARGET = libpcp_web.$(DSOSUFFIX)
endif
ifeq "$(TARGET_OS)" "mingw"
LIBTARGET = libpcp_web.$(DSOSUFFIX)
STATICLIBTARGET =
SYMTARGET =
endif

ifeq "$(ENABLE_SHARED)" "no"
LIBTARGET =
SYMTARGET =
endif

VERSION_SCRIPT = exports
LDIRT = $(XFILES) $(YFILES) $(LIBVALKEY_XFILES) $(SYMTARGET) $(YFILES:%.y=%.tab.?)
ifneq "$(HAVE_LIBINIH)" "true"
	LDIRT += $(INIH_XFILES)
endif

default: $(XFILES) $(LIBVALKEY_XFILES) $(INIH_XFILES) $(LIBTARGET) $(SYMTARGET) $(STATICLIBTARGET)

include $(BUILDRULES)

ifneq ($(SYMTARGET),)
$(SYMTARGET):
	$(LN_S) -f $(LIBTARGET) $@
endif

install: default
	$(INSTALL) -m 644 -t $(REWRITEVARDIR)/pmproxy_fixups.conf logrewrite.conf $(REWRITEDIR)/pmproxy_fixups.conf
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
ifneq ($(INIH_XFILES),)
$(INIH_XFILES):
	mkdir -p deps/inih
	$(LN_S) -f $(realpath $(TOPDIR))/vendor/github.com/benhoyt/$(@:deps/%=%) $@
endif
$(LIBVALKEY_XFILES):
	mkdir -p deps/libvalkey/include/valkey/adapters
	mkdir -p deps/libvalkey/src
	$(LN_S) -f $(realpath $(TOPDIR))/vendor/github.com/valkey-io/$(@:deps/%=%) $@

.NOTPARALLEL:
query_parser.tab.h query_parser.tab.c: query_parser.y query.h
	$(YACC) -d -b `basename $< .y` $<

query_parser.y: query_parser.y.in fix_query_parser_y
	./fix_query_parser_y

default_pcp:	default

install_pcp:	install

ifneq ($(LIBTARGET),)
$(LIBTARGET): $(VERSION_SCRIPT) $(XFILES) $(LIBVALKEY_XFILES) $(INIH_XFILES)
endif

%.o: $(LIBVALKEY_XFILES) $(INIH_XFILES)

jsmn.o:		jsmn.c jsmn.h
discover.o:	discover.h discover.c
http_parser.o:	http_parser.c http_parser.h
query.o:	query.h query.c query_parser.y
http_client.o load.o query.o:	$(TOPDIR)/src/include/pcp/libpcp.h

exports: exports.in ../../libpcp/src/mk.exports
	../../libpcp/src/mk.exports

debug:
	@echo TARGET_OS=$(TARGET_OS)
	@echo HAVE_LIBINIH=$(HAVE_LIBINIH)
	@echo LLDLIBS=$(LLDLIBS)
