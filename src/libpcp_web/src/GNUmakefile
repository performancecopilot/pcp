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

HIREDIS_HFILES = $(addprefix deps/hiredis/, \
	alloc.h async.h async_private.h fmacros.h hiredis.h net.h \
	read.h sockcompat.h win32.h adapters/libuv.h)
HIREDIS_CFILES = $(addprefix deps/hiredis/, \
	alloc.c async.c hiredis.c net.c read.c)
HIREDIS_XFILES = $(HIREDIS_HFILES) $(HIREDIS_CFILES)

HIREDIS_CLUSTER_HFILES = $(addprefix deps/hiredis-cluster/, \
	adlist.h command.h hiarray.h hiutil.h hircluster.h win32.h \
	adapters/libuv.h)
HIREDIS_CLUSTER_CFILES = $(addprefix deps/hiredis-cluster/, \
	adlist.c command.c crc16.c hiarray.c hiutil.c hircluster.c)
HIREDIS_CLUSTER_XFILES = $(HIREDIS_CLUSTER_HFILES) $(HIREDIS_CLUSTER_CFILES)

CFILES = jsmn.c http_client.c http_parser.c sds.c siphash.c \
	 query.c schema.c load.c sha1.c util.c slots.c \
	 redis.c dict.c ini.c maps.c batons.c encoding.c \
	 search.c json_helpers.c config.c \
	 $(HIREDIS_CFILES) $(HIREDIS_CLUSTER_CFILES)
HFILES = jsmn.h http_client.h http_parser.h sdsalloc.h zmalloc.h \
	 query.h schema.h load.h sha1.h util.h slots.h \
	 redis.h dict.h ini.h maps.h batons.h encoding.h \
	 search.h discover.h private.h
YFILES = query_parser.y
XFILES = jsmn.c jsmn.h http_parser.c http_parser.h \
	 sha1.c sha1.h sds.c siphash.c dict.c dict.h ini.c ini.h

LLDLIBS = $(PCPWEBLIB_EXTRAS) $(LIB_FOR_MATH)
ifeq "$(TARGET_OS)" "mingw"
LLDLIBS += -lws2_32
endif

LDIRT = $(XFILES) $(YFILES:%.y=%.tab.?)
LCFLAGS += $(C99_CFLAGS) -DJSMN_PARENT_LINKS=1 -DJSMN_STRICT=1 -DHTTP_PARSER_STRICT=0 -Ideps

ifeq "$(HAVE_LIBUV)" "true"
CFILES += discover.c webgroup.c timer.c
LCFLAGS += $(LIBUVCFLAGS) -DHAVE_LIBUV=1
LLDLIBS += $(LIB_FOR_LIBUV)
else
CFILES += nodiscover.c nowebgroup.c notimer.c
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
LDIRT = $(XFILES) $(HIREDIS_XFILES) $(HIREDIS_CLUSTER_XFILES) $(YFILES:%.y=%.tab.?)

default: $(XFILES) $(HIREDIS_XFILES) $(HIREDIS_CLUSTER_XFILES) $(LIBTARGET) $(SYMTARGET) $(STATICLIBTARGET)

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
$(HIREDIS_XFILES):
	mkdir -p deps/hiredis/adapters
	$(LN_S) -f $(realpath $(TOPDIR))/vendor/github.com/redis/$(@:deps/%=%) $@
$(HIREDIS_CLUSTER_XFILES):
	mkdir -p deps/hiredis-cluster/adapters
	$(LN_S) -f $(realpath $(TOPDIR))/vendor/github.com/Nordix/$(@:deps/%=%) $@

.NOTPARALLEL:
query_parser.tab.h query_parser.tab.c: query_parser.y query.h
	$(YACC) -d -b `basename $< .y` $<

default_pcp:	default

install_pcp:	install

ifneq ($(LIBTARGET),)
$(LIBTARGET): $(VERSION_SCRIPT) $(XFILES) $(HIREDIS_XFILES) $(HIREDIS_CLUSTER_XFILES)
endif

jsmn.o:		jsmn.c jsmn.h
discover.o:	discover.h discover.c
http_parser.o:	http_parser.c http_parser.h
http_client.o load.o query.o query_parser.o:	$(TOPDIR)/src/include/pcp/libpcp.h

check:: $(CFILES) $(HFILES) $(XFILES)
	$(CLINT) \
	  --suppress=unknownMacro:/usr/include/bits/stdlib.h \
	  --suppress=internalAstError:deps/hiredis-cluster/command.c:351 \
	  $^
