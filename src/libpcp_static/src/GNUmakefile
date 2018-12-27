#
# Copyright (c) 2012-2018 Red Hat.
# Copyright (c) 2008 Aconex.  All Rights Reserved.
# Copyright (c) 2000,2003,2004 Silicon Graphics, Inc.  All Rights Reserved.
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
include ../../libpcp/src/GNUlibrarydefs
-include ./GNUlocaldefs

CFILES = connect.c context.c desc.c err.c fetch.c fetchgroup.c freeresult.c \
	help.c instance.c labels.c p_desc.c p_error.c p_fetch.c p_instance.c \
	p_profile.c p_result.c p_text.c p_pmns.c p_creds.c p_attr.c p_label.c \
	pdu.c pdubuf.c pmns.c profile.c store.c units.c util.c ipc.c \
	sortinst.c logmeta.c logportmap.c logutil.c tz.c interp.c \
	rtime.c tv.c spec.c fetchlocal.c optfetch.c AF.c \
	stuffvalue.c endian.c config.c auxconnect.c auxserver.c discovery.c \
	p_lcontrol.c p_lrequest.c p_lstatus.c logconnect.c logcontrol.c \
	connectlocal.c derive_fetch.c events.c lock.c hash.c jsmn.c \
	fault.c access.c getopt.c io.c io_stdio.c exec.c \
	shellprobe.c subnetprobe.c \
	deprecated.c
HFILES = derive.h internal.h compiler.h pmdbg.h \
	avahi.h subnetprobe.h shellprobe.h
EXT_FILES = jsmn.h jsmn.c sort_r.h
YFILES = getdate.y derive_parser.y
GENERATED_HFILES = pmdbg.h

# files that will be linked to here from libpcp directory
LN_FILES = $(filter-out $(EXT_FILES),$(CFILES)) $(HFILES) $(YFILES)

LCFLAGS = '-DEXEC_SUFFIX="$(EXECSUFFIX)"' '-DDSO_SUFFIX="$(DSOSUFFIX)"'
LCFLAGS += $(LIBPCP_CFLAGS) -DPCP_STATIC
LLDLIBS += $(LIBPCP_LDLIBS)

ifeq "$(ENABLE_SECURE)" "true"
CFILES += secureserver.c secureconnect.c
endif

ifeq "$(ENABLE_AVAHI)" "true"
CFILES += avahi.c
endif

ifeq "$(ENABLE_LZMA)" "true"
CFILES += io_xz.c
endif

ifneq "$(TARGET_OS)" "mingw"
CFILES += accounts.c
LLDLIBS	+= -lpsapi -lws2_32 -liphlpapi
else
CFILES += win32.c
endif

ifeq "$(LIB_FOR_BASENAME)" "-lpcp"
# don't need to be linked to myself in this case!
LIB_FOR_BASENAME =
endif

DSOVERSION = 3
STATICLIBTARGET = libpcp_static.a
LIBTARGET =
SYMTARGET =

LDIRT += $(SYMTARGET) $(YFILES:%.y=%.tab.?) getdate.h check.done \
	$(LN_FILES) $(EXT_FILES) exports *.c

base default : $(HFILES) $(CFILES) $(EXT_FILES) $(VERSION_SCRIPT) $(INFILES) \
	$(LIBTARGET) $(SYMTARGET) $(STATICLIBTARGET)


include $(BUILDRULES)

*.o: internal.h compiler.h derive.h
jsmn.o: jsmn.c jsmn.h
rtime.o: getdate.h
derive.o derive_fetch.o: derive.h derive_parser.tab.h
util.o:	pmdbg.h
labels.o:	sort_r.h

$(OBJECTS):	$(TOPDIR)/src/include/pcp/pmapi.h \
	$(TOPDIR)/src/include/pcp/platform_defs.h

.NOTPARALLEL:
getdate.h getdate.tab.c: getdate.y
	$(YACC) -d -b `basename $< .y` $< && cp `basename $@ .h`.tab.h $@

install : default

# kernel_pmda_dso	= windows

$(LN_FILES):
	rm -f $@
	ln -s $(TOPDIR)/src/libpcp/src/$@ .

$(EXT_FILES):
	rm -f $@
	ln $(TOPDIR)/src/external/$@ .
