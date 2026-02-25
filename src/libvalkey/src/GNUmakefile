#
# Copyright (c) 2026 Red Hat.
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

LIBVALKEY_HFILES = $(addprefix deps/libvalkey/, \
	include/valkey/alloc.h include/valkey/async.h include/valkey/cluster.h \
	include/valkey/valkey.h include/valkey/net.h include/valkey/read.h \
	include/valkey/visibility.h include/valkey/sockcompat.h \
	include/valkey/adapters/libuv.h \
	src/adlist.h src/async_private.h src/cmddef.h src/command.h src/dict.h \
	src/fmacros.h src/sds.h src/sdsalloc.h src/valkey_private.h \
	src/vkutil.h src/win32.h)
LIBVALKEY_CFILES = $(addprefix deps/libvalkey/, \
	src/adlist.c src/alloc.c src/async.c src/cluster.c src/command.c \
	src/conn.c src/crc16.c src/dict.c src/net.c src/read.c src/sds.c \
	src/sockcompat.c src/valkey.c src/vkutil.c)
LIBVALKEY_XFILES = $(LIBVALKEY_HFILES) $(LIBVALKEY_CFILES)

CFILES = $(LIBVALKEY_CFILES)

STATICLIBTARGET = libvalkey.a

LCFLAGS = -U_GNU_SOURCE -Ideps -Ideps/libvalkey/include/valkey \
	  -Ideps/libvalkey/include -Ideps/libvalkey/src

LDIRT = $(LIBVALKEY_XFILES)

default: $(LIBVALKEY_XFILES) $(STATICLIBTARGET)

include $(BUILDRULES)

deps/libvalkey/include/valkey/adapters deps/libvalkey/include/valkey deps/libvalkey/src:
	mkdir -p $@

$(LIBVALKEY_XFILES): | deps/libvalkey/include/valkey/adapters deps/libvalkey/include/valkey deps/libvalkey/src
	$(LN_S) -f $(realpath $(TOPDIR))/vendor/github.com/valkey-io/$(@:deps/%=%) $@

%.o: $(LIBVALKEY_XFILES)

install: default
	$(INSTALL) -m 755 $(STATICLIBTARGET) $(PCP_LIB_DIR)/$(STATICLIBTARGET)

default_pcp: default

install_pcp: install
